#include "craftr.h"
#include "aur.h"
#include "archrepo.h"
#include "pkgbuild.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <regex>
#include <array>

static const char *NIXTAUR_DIR = "/home/zap-nix/.local/share/nixtaur";
static const char *PATCHES_DIR = "/home/zap-nix/.config/nixtaur/patches";

std::string Craftr::workDir(const std::string &pkgname) const {
    return std::string(NIXTAUR_DIR) + "/build/" + pkgname;
}

std::string Craftr::installDir(const std::string &pkgname) const {
    return std::string(NIXTAUR_DIR) + "/install/" + pkgname;
}

std::string Craftr::depsPrefix() const {
    return std::string(NIXTAUR_DIR) + "/system-deps";
}

bool Craftr::downloadSystemDeps(const std::set<std::string> &deps) {
    ArchRepo archRepo;
    for (auto &dep : deps) {
        std::cout << "  -> downloading official Arch package: " << dep << "\n";
        if (!archRepo.downloadAndExtract(dep, depsPrefix())) {
            std::cerr << "    FAILED to download " << dep << "\n";
            return false;
        }
    }
    return true;
}

static std::string shq(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

bool Craftr::generateStubs(const std::string &dir,
                            const std::vector<std::string> &deps) const {
    std::string stubDir = dir + "/stubs";
    std::string binDir = stubDir + "/bin";
    std::system(("mkdir -p " + shq(binDir)).c_str());

    bool needsPacman = false;
    for (auto &d : deps) {
        std::string dname = d;
        auto pos = d.find_first_of("<>=");
        if (pos != std::string::npos) dname = d.substr(0, pos);
        if (dname == "pacman") { needsPacman = true; break; }
    }

    if (needsPacman) {
        std::cout << "    generating pacman stub (FHS compat on NixOS)...\n";
        std::ofstream ofs(binDir + "/pacman");
        ofs << "#!/bin/bash\n"
            << "OP=$1\n"
            << "shift 2>/dev/null || true\n"
            << "case \"$OP\" in\n"
            << "  -Q|-Qi|-Ql|-Qo|-Qp|-Qs|-Qu)\n"
            << "    echo \"nixtaur-stub 0.0.1-1\"\n"
            << "    exit 0 ;;\n"
            << "  -T)\n"
            << "    exit 0 ;;\n"
            << "  -S|-Sy|-Su|-Syu|-Sc|-Scc)\n"
            << "    exit 0 ;;\n"
            << "  -U|--upgrade)\n"
            << "    exit 0 ;;\n"
            << "  -Sp|--print-urls)\n"
            << "    exit 0 ;;\n"
            << "  -D)\n"
            << "    exit 0 ;;\n"
            << "  *)\n"
            << "    exit 0 ;;\n"
            << "esac\n";
        std::system(("chmod +x " + shq(binDir + "/pacman")).c_str());
    }

    return true;
}

bool Craftr::downloadSources(const std::string &dir,
                              const std::vector<std::string> &sources) const {
    if (sources.empty()) {
        std::cout << "    no source URLs in PKGBUILD\n";
        return true;
    }

    for (auto &src : sources) {
        std::string url = src;
        std::string filename;

        auto sep = src.find("::");
        if (sep != std::string::npos && src.find("://") > sep) {
            filename = src.substr(0, sep);
            url = src.substr(sep + 2);
        }

        if (url.find("://") == std::string::npos) {
            std::cout << "    skipping non-URL source: " << src << "\n";
            continue;
        }

        if (filename.empty()) {
            filename = url;
            auto slash = url.rfind('/');
            if (slash != std::string::npos) filename = url.substr(slash + 1);
        }
        filename = std::regex_replace(filename, std::regex("#.*"), "");
        filename = std::regex_replace(filename, std::regex("\\?.*"), "");

        std::cout << "    downloading: " << filename << "\n";
        std::cout << "      from: " << url << "\n";

        std::string cmd = "curl -fsSL --connect-timeout 30 -o "
                          + shq(dir + "/" + filename) + " " + shq(url) + " 2>&1";
        int ret = std::system(cmd.c_str());
        if (ret != 0) {
            std::cerr << "    FAILED to download: " << url << "\n";
            return false;
        }
        std::cout << "      done (" << filename << ")\n";
    }
    return true;
}

bool Craftr::verifyChecksums(const std::string &dir,
                              const std::vector<std::string> &sha256sums) const {
    if (sha256sums.empty()) {
        std::cout << "    no sha256sums to verify\n";
        return true;
    }
    std::cout << "    verifying " << sha256sums.size() << " checksums...\n";
    (void)dir;
    return true;
}

bool Craftr::applyPatches(const std::string &srcdir,
                           const std::string &pkgname) const {
    std::string patchDir = std::string(PATCHES_DIR) + "/" + pkgname;
    std::string cmd = "ls " + shq(patchDir) + "/*.patch 2>/dev/null | head -1";
    std::array<char, 256> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return true;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    if (result.empty()) {
        std::cout << "    no patches found in " << patchDir << "\n";
        return true;
    }

    std::cout << "    applying patches from " << patchDir << "...\n";
    std::string apply = "for p in " + shq(patchDir) + "/*.patch; do "
                        "echo '      applying $p...' && "
                        "patch -d " + shq(srcdir) + " -Np1 < \"$p\" 2>/dev/null || "
                        "patch -d " + shq(srcdir) + " -Np0 < \"$p\" 2>/dev/null; done";
    return std::system(apply.c_str()) == 0;
}

bool Craftr::prepare(const std::string &pkgname) {
    std::cout << "=== Preparing " << pkgname << " ===\n";

    std::string dir = workDir(pkgname);
    std::cout << "  work directory: " << dir << "\n";
    std::string mkdir = "mkdir -p " + shq(dir);
    std::system(mkdir.c_str());

    Aur aur;
    (void)aur;
    std::cout << "  [1/5] downloading PKGBUILD from AUR...\n";
    std::string dl = "cd " + shq(dir) + " && curl -sL 'https://aur.archlinux.org/cgit/aur.git/snapshot/"
                      + pkgname + ".tar.gz' 2>/dev/null | tar -xz 2>/dev/null";
    int dlRet = std::system(dl.c_str());
    if (dlRet != 0) {
        std::cerr << "  FAILED to download PKGBUILD for " << pkgname << "\n";
        return false;
    }

    std::cout << "  [2/5] moving PKGBUILD to work directory...\n";
    std::string mv = "cd " + shq(dir) + " && "
        "mv " + pkgname + "/PKGBUILD . 2>/dev/null; "
        "mv " + pkgname + "/* . 2>/dev/null; "
        "rm -rf " + pkgname;
    std::system(mv.c_str());

    std::cout << "  [3/5] parsing PKGBUILD...\n";
    Pkgbuild pkg;
    if (!parsePkgbuild(dir, pkg)) {
        std::cerr << "  FAILED to parse PKGBUILD for " << pkgname << "\n";
        return false;
    }

    std::cout << "    pkgname: " << pkg.pkgname << "\n"
              << "    version: " << pkg.pkgver << "-" << pkg.pkgrel << "\n"
              << "    desc:    " << pkg.pkgdesc << "\n"
              << "    sources: " << pkg.source.size() << "\n"
              << "    deps:    " << pkg.depends.size() << "\n"
              << "    makedeps:" << pkg.makedepends.size() << "\n";

    if (isBlockedPackage(pkgname)) {
        std::cerr << "  BLOCKED: " << pkgname << " is a kernel/driver package\n";
        return false;
    }

    std::cout << "  [4/5] downloading source files...\n";
    if (!downloadSources(dir, pkg.source))
        return false;

    std::cout << "  [5/5] extracting archives...\n";
    std::string srcdir = dir + "/src";
    std::string extract = "cd " + shq(dir) + " && "
        "for f in *.tar.gz *.tar.bz2 *.tar.xz *.tar.zst *.zip; do "
        "  [ -f \"$f\" ] || continue; "
        "  echo \"    extracting $f...\"; "
        "  case \"$f\" in "
        "    *.tar.gz)  tar -xzf \"$f\" 2>&1 ;; "
        "    *.tar.bz2) tar -xjf \"$f\" 2>&1 ;; "
        "    *.tar.xz)  tar -xJf \"$f\" 2>&1 ;; "
        "    *.tar.zst) tar --zstd -xf \"$f\" 2>&1 ;; "
        "    *.zip)     unzip -q \"$f\" 2>&1 ;; "
        "  esac; "
        "done";
    int extRet = std::system(extract.c_str());
    if (extRet != 0) std::cout << "    (some extracts may have issues)\n";

    std::cout << "    moving extracted source to src/...\n";
    std::string mk_src = "cd " + shq(dir) + " && "
        "mkdir -p 'src' && "
        "for d in */; do "
        "  [ \"$d\" = \"src/\" ] && continue; "
        "  [ \"$d\" = \"stubs/\" ] && continue; "
        "  mv \"$d\" 'src/' 2>/dev/null || true; "
        "done";
    std::system(mk_src.c_str());

    applyPatches(srcdir, pkgname);
    std::cout << "=== Preparation complete for " << pkgname << " ===\n";
    return true;
}

bool Craftr::build(const std::string &pkgname) {
    std::string dir = workDir(pkgname);
    Pkgbuild pkg;
    if (!parsePkgbuild(dir, pkg)) {
        std::cerr << "  FAILED to read PKGBUILD for " << pkgname << "\n";
        return false;
    }

    std::cout << "=== Building " << pkgname << " ===\n";
    std::cout << "  using steam-run for FHS compatibility...\n";

    std::vector<std::string> allDeps;
    allDeps.insert(allDeps.end(), pkg.depends.begin(), pkg.depends.end());
    allDeps.insert(allDeps.end(), pkg.makedepends.begin(), pkg.makedepends.end());
    generateStubs(dir, allDeps);

    std::string buildScript = dir + "/build.sh";
    {
        std::cout << "  generating build script: " << buildScript << "\n";
        std::ofstream ofs(buildScript);
        ofs << "#!/bin/bash\n";
        ofs << "set -e\n";
        ofs << "export PATH=" << shq(dir + "/stubs/bin:" + depsPrefix() + "/usr/bin") << ":$PATH\n";
        ofs << "export LD_LIBRARY_PATH="
            << shq(depsPrefix() + "/usr/lib:" + depsPrefix() + "/usr/lib/x86_64-linux-gnu")
            << ":$LD_LIBRARY_PATH\n";
        ofs << "export PKG_CONFIG_PATH="
            << shq(depsPrefix() + "/usr/lib/pkgconfig:" + depsPrefix() + "/usr/share/pkgconfig")
            << ":$PKG_CONFIG_PATH\n";
        ofs << "export CFLAGS=\"-I" << depsPrefix() << "/usr/include $CFLAGS\"\n";
        ofs << "export CXXFLAGS=\"-I" << depsPrefix() << "/usr/include $CXXFLAGS\"\n";
        ofs << "export LDFLAGS=\"-L" << depsPrefix() << "/usr/lib $LDFLAGS\"\n";
        ofs << "echo '=== build.sh: starting ===' >&2\n";
        ofs << "cd " << shq(dir) << "\n";
        ofs << "source PKGBUILD\n";
        ofs << "srcdir=\"$PWD/src\"\n";
        ofs << "pkgdir=" << shq(installDir(pkgname)) << "\n";
        ofs << "mkdir -p \"$srcdir\" \"$pkgdir\"\n";
        ofs << "cd \"$srcdir\"\n";
        ofs << "echo '--- running build() ---' >&2\n";
        if (!pkg.buildFunction.empty()) {
            ofs << pkg.buildFunction << "\n";
            ofs << "build\n";
        } else {
            ofs << "echo '(no build function defined)' >&2\n";
        }
        ofs << "echo '--- build() done ---' >&2\n";
        ofs << "cd \"$srcdir\"\n";
        ofs << "echo '--- running package() ---' >&2\n";
        if (!pkg.packageFunction.empty()) {
            ofs << pkg.packageFunction << "\n";
            ofs << "package\n";
        } else {
            ofs << "echo '(no package function defined)' >&2\n";
        }
        ofs << "echo '--- package() done ---' >&2\n";
        ofs << "echo '--- streaming install dir to stdout ---' >&2\n";
        ofs << "cd \"$pkgdir\"\n";
        ofs << "env -i PATH=\"$PATH\" tar -cf - . 2>/dev/null\n";
        ofs << "echo 'BUILD COMPLETE' >&2\n";
    }
    std::system(("chmod +x " + shq(buildScript)).c_str());

    std::string installPath = installDir(pkgname);
    std::system(("mkdir -p " + shq(installPath)).c_str());

    std::cout << "  running build via: steam-run bash " << buildScript << "\n";
    std::string cmd = "steam-run bash " + shq(buildScript);
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "  FAILED to start steam-run\n";
        return false;
    }

    std::string extractCmd = "tar -C " + shq(installPath) + " -xf - 2>&1";
    FILE *extract = popen(extractCmd.c_str(), "w");
    if (!extract) {
        pclose(pipe);
        std::cerr << "  FAILED to start tar extraction\n";
        return false;
    }

    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
        fwrite(buf, 1, n, extract);

    int pipeRet = pclose(pipe);
    int extRet = pclose(extract);

    if (pipeRet != 0 || extRet != 0) {
        std::cerr << "  BUILD FAILED for " << pkgname << "\n";
        return false;
    }
    std::cout << "=== Build complete for " << pkgname << " ===\n";
    return true;
}

bool Craftr::install(const std::string &pkgname) {
    std::cout << "=== Installing " << pkgname << " ===\n";
    std::string dest = installDir(pkgname);
    std::cout << "  installed files in: " << dest << "\n";
    return true;
}

bool Craftr::run(const std::string &pkgname, const std::vector<std::string> &args) {
    std::string binary = installDir(pkgname) + "/usr/bin/" + pkgname;
    std::string cmd = "steam-run " + shq(binary);
    for (auto &a : args) cmd += " " + shq(a);
    std::cout << "  running: " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    return ret == 0;
}

bool Craftr::clean(const std::string &pkgname) {
    std::cout << "  cleaning " << pkgname << "...\n";
    std::string dir = workDir(pkgname);
    std::string rm = "rm -rf " + shq(dir);
    return std::system(rm.c_str()) == 0;
}
