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
#include <sys/wait.h>

static std::string nixtaurDir() {
    const char *home = getenv("HOME");
    return (home ? std::string(home) : "/tmp") + "/.local/share/nixtaur";
}
static std::string patchesDir() {
    const char *home = getenv("HOME");
    return (home ? std::string(home) : "/tmp") + "/.config/nixtaur/patches";
}
static std::string cacheDir() {
    return nixtaurDir() + "/.cache/arch-pkgs";
}

std::string Craftr::workDir(const std::string &pkgname) const {
    return nixtaurDir() + "/build/" + pkgname;
}

std::string Craftr::installDir(const std::string &pkgname) const {
    return nixtaurDir() + "/install/" + pkgname;
}

std::string Craftr::depsPrefix() const {
    return nixtaurDir() + "/system-deps";
}

static std::string shq(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

bool Craftr::downloadSystemDeps(const std::set<std::string> &deps) {
    ArchRepo archRepo;
    std::string mkdir = "mkdir -p " + shq(cacheDir());
    std::system(mkdir.c_str());
    for (auto &dep : deps) {
        std::cout << "  -> downloading official Arch package: " << dep << "\n";
        if (!archRepo.downloadAndExtract(dep, depsPrefix(), cacheDir())) {
            std::cerr << "    FAILED to download " << dep << "\n";
            return false;
        }
        auto info = archRepo.search(dep);
        if (info.found && !info.depends.empty()) {
            std::cout << "    downloading runtime dependencies of " << dep << "...\n";
            for (auto &rdep : info.depends) {
                std::string depName = rdep;
                auto pos = depName.find_first_of("<>=");
                if (pos != std::string::npos) depName = depName.substr(0, pos);
                if (!depName.empty() && deps.find(depName) == deps.end()) {
                    if (!archRepo.downloadAndExtract(depName, depsPrefix(), cacheDir())) {
                        // Retry: strip .so suffix to find real package name
                        std::string pkgName;
                        auto soPos = depName.find(".so");
                        if (soPos != std::string::npos)
                            pkgName = depName.substr(0, soPos);
                        if (!pkgName.empty() && pkgName != depName) {
                            if (!archRepo.downloadAndExtract(pkgName, depsPrefix(), cacheDir()))
                                std::cerr << "    WARNING: could not download runtime dep "
                                          << depName << " (tried as " << pkgName << ")\n";
                        } else {
                            std::cerr << "    WARNING: could not download runtime dep "
                                      << depName << "\n";
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool Craftr::installSystemPackage(const std::string &pkgname) {
    std::cout << "=== Installing official Arch package: " << pkgname << " ===\n";
    ArchRepo archRepo;

    auto info = archRepo.search(pkgname);
    if (!info.found) {
        std::cerr << "  FAILED: " << pkgname << " not found in Arch repos\n";
        return false;
    }

    if (!info.depends.empty()) {
        std::cout << "  installing runtime dependencies to shared prefix...\n";
        for (auto &dep : info.depends) {
            std::string depName = dep;
            auto pos = depName.find_first_of("<>=");
            if (pos != std::string::npos) depName = depName.substr(0, pos);
            if (!depName.empty()) {
                if (!archRepo.downloadAndExtract(depName, depsPrefix(), cacheDir()))
                    std::cerr << "  WARNING: could not install runtime dep " << depName << "\n";
            }
        }
    }

    std::string dest = installDir(pkgname);
    std::string mkdir = "mkdir -p " + shq(dest);
    if (std::system(mkdir.c_str()) != 0) {
        std::cerr << "  FAILED to create install directory\n";
        return false;
    }
    return archRepo.downloadAndExtract(pkgname, dest, cacheDir());
}

bool Craftr::generateStubs(const std::string &dir,
                            const std::vector<std::string> &deps) const {
    std::string stubDir = dir + "/stubs";
    std::string binDir = stubDir + "/bin";
    std::system(("mkdir -p " + shq(binDir)).c_str());

    bool needsPacman = false;
    bool needsMakepkg = false;
    for (auto &d : deps) {
        std::string dname = d;
        auto pos = d.find_first_of("<>=");
        if (pos != std::string::npos) dname = d.substr(0, pos);
        if (dname == "pacman") { needsPacman = true; }
        if (dname == "makepkg") { needsMakepkg = true; }
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

    if (needsMakepkg) {
        std::cout << "    generating makepkg stub (FHS compat on NixOS)...\n";
        std::ofstream ofs(binDir + "/makepkg");
        ofs << "#!/bin/bash\n"
            << "case \"$1\" in\n"
            << "  -g|--geninteg)\n"
            << "    exit 0 ;;\n"
            << "  --printsrcinfo)\n"
            << "    exit 0 ;;\n"
            << "  --nobuild|-o)\n"
            << "    exit 0 ;;\n"
            << "  --install|-i)\n"
            << "    shift; exec \"$@\" ;;\n"
            << "  *)\n"
            << "    exit 0 ;;\n"
            << "esac\n";
        std::system(("chmod +x " + shq(binDir + "/makepkg")).c_str());
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

        std::string cmd = "curl -fL --proto =https --connect-timeout 30 --progress-bar -o "
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
                              const std::vector<std::string> &sources,
                              const std::vector<std::string> &sha256sums) const {
    if (sha256sums.empty()) {
        std::cout << "    no sha256sums to verify\n";
        return true;
    }
    std::cout << "    verifying " << sha256sums.size() << " checksums...\n";

    for (size_t i = 0; i < sha256sums.size(); i++) {
        std::string expected = sha256sums[i];
        if (expected == "SKIP") continue;

        std::string url = (i < sources.size()) ? sources[i] : "";
        std::string filename = url;

        auto sep = url.find("::");
        if (sep != std::string::npos && url.find("://") > sep)
            filename = url.substr(0, sep);

        if (filename.find("://") != std::string::npos || filename == url) {
            auto slash = filename.rfind('/');
            if (slash != std::string::npos) filename = filename.substr(slash + 1);
        }
        filename = std::regex_replace(filename, std::regex("#.*"), "");
        filename = std::regex_replace(filename, std::regex("\\?.*"), "");

        if (filename.find("://") != std::string::npos || filename.empty())
            continue;

        std::string filepath = dir + "/" + filename;
        std::string cmd = "sha256sum " + shq(filepath) + " 2>/dev/null";
        std::array<char, 128> buf;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            std::cerr << "    WARNING: could not verify " << filename << "\n";
            continue;
        }
        if (fgets(buf.data(), buf.size(), pipe.get()) != nullptr)
            result = buf.data();

        std::string actual = result.substr(0, result.find(' '));
        if (actual != expected) {
            std::cerr << "    CHECKSUM MISMATCH for " << filename << "\n"
                      << "      expected: " << expected << "\n"
                      << "      actual:   " << actual << "\n";
            return false;
        }
        std::cout << "      " << filename << ": OK\n";
    }
    return true;
}

bool Craftr::applyPatches(const std::string &srcdir,
                           const std::string &pkgname) const {
    std::string patchDir = patchesDir() + "/" + pkgname;
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

    std::cout << "  [1/5] downloading PKGBUILD from AUR...\n";
    std::string dl = "cd " + shq(dir) + " && curl -sL --proto =https 'https://aur.archlinux.org/cgit/aur.git/snapshot/"
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

    std::string checkPkg = "test -f " + shq(dir + "/PKGBUILD");
    if (std::system(checkPkg.c_str()) != 0) {
        std::cerr << "  FAILED: PKGBUILD not found after extraction\n";
        return false;
    }

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

    std::cout << "  [4b/5] verifying checksums...\n";
    if (!verifyChecksums(dir, pkg.source, pkg.sha256sums))
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
        ofs << "export PATH=" << shq(dir + "/stubs/bin:" + depsPrefix() + "/usr/bin:" + depsPrefix() + "/usr/lib/go/bin:" + depsPrefix() + "/usr/local/bin") << ":$PATH\n";
        ofs << "export LD_LIBRARY_PATH="
            << shq(depsPrefix() + "/usr/lib:" + depsPrefix() + "/usr/lib/x86_64-linux-gnu")
            << ":$LD_LIBRARY_PATH\n";
        ofs << "export PKG_CONFIG_PATH="
            << shq(depsPrefix() + "/usr/lib/pkgconfig:" + depsPrefix() + "/usr/share/pkgconfig")
            << ":$PKG_CONFIG_PATH\n";
        ofs << "export LIBRARY_PATH=" << shq(depsPrefix() + "/usr/lib") << ":$LIBRARY_PATH\n";
        ofs << "export CFLAGS=\"-idirafter " << depsPrefix() << "/usr/include $CFLAGS\"\n";
        ofs << "export CXXFLAGS=\"-idirafter " << depsPrefix() << "/usr/include $CXXFLAGS\"\n";
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
            ofs << "build >&2\n";
        } else {
            ofs << "echo '(no build function defined)' >&2\n";
        }
        ofs << "echo '--- build() done ---' >&2\n";
        ofs << "cd \"$srcdir\"\n";
        ofs << "echo '--- running package() ---' >&2\n";
        if (!pkg.packageFunction.empty()) {
            ofs << pkg.packageFunction << "\n";
            ofs << "package >&2\n";
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

bool Craftr::isInstalled(const std::string &pkgname) const {
    std::string check = "test -d " + shq(installDir(pkgname)) + " 2>/dev/null";
    return std::system(check.c_str()) == 0;
}

bool Craftr::install(const std::string &pkgname) {
    std::cout << "=== Installing " << pkgname << " ===\n";
    std::string dest = installDir(pkgname);
    std::cout << "  installed files in: " << dest << "\n";
    return true;
}

bool Craftr::run(const std::string &pkgname, const std::vector<std::string> &args) {
    if (!isInstalled(pkgname)) {
        std::cerr << "error: " << pkgname << " is not installed\n"
                  << "  Install it first with: nixtaur -S " << pkgname << "\n";
        return false;
    }

    Runner runner;
    return runner.runPackage(pkgname, args);
}

bool Craftr::clean(const std::string &pkgname) {
    std::cout << "  removing " << pkgname << "...\n";
    bool ok = true;
    ok &= std::system(("rm -rf " + shq(workDir(pkgname))).c_str()) == 0;
    ok &= std::system(("rm -rf " + shq(installDir(pkgname))).c_str()) == 0;
    return ok;
}
