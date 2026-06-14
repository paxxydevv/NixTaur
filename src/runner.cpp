#include "runner.h"
#include "pkgbuild.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <array>
#include <memory>
#include <sys/wait.h>

static std::string cachedHome() {
    static std::string home;
    if (home.empty()) {
        const char *h = getenv("HOME");
        home = h ? h : "/tmp";
    }
    return home;
}

static std::string nixtaurDir() {
    return cachedHome() + "/.local/share/nixtaur";
}

std::string Runner::depsPrefix() const {
    return nixtaurDir() + "/system-deps";
}

std::string Runner::installDir(const std::string &pkgname) const {
    return nixtaurDir() + "/install/" + pkgname;
}

std::string Runner::sandboxDir(const std::string &pkgname) const {
    return sandboxRoot_ + "/" + pkgname;
}

static std::string shq(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out + "'";
}

Runner::Runner() {
    const char *home = getenv("HOME");
    sandboxRoot_ = (home ? std::string(home) : "/tmp") + "/.local/share/nixtaur/sandbox";
}

Runner::~Runner() {}

bool Runner::setupSandbox(const std::string &pkgname) {
    std::string dir = sandboxDir(pkgname);
    std::string cmd = "mkdir -p " + shq(dir + "/tmp") + " " + shq(dir + "/config") + " " + shq(dir + "/data");
    std::system(cmd.c_str());
    return true;
}

void Runner::cleanSandbox(const std::string &pkgname) {
    std::string dir = sandboxDir(pkgname);
    std::system(("rm -rf " + shq(dir)).c_str());
}

std::string Runner::findBinary(const std::string &pkgname) {
    std::string base = installDir(pkgname);

    std::vector<std::string> candidates = {
        base + "/usr/bin/" + pkgname,
        base + "/usr/local/bin/" + pkgname,
        base + "/bin/" + pkgname,
    };
    for (auto &c : candidates) {
        std::string check = "test -x " + shq(c) + " 2>/dev/null";
        if (std::system(check.c_str()) == 0)
            return c;
    }

    std::string searchDirs =
        shq(base + "/usr/bin") + " " +
        shq(base + "/usr/local/bin") + " " +
        shq(base + "/bin");
    std::string findCmd = "find " + searchDirs + " -type f -executable 2>/dev/null";
    std::array<char, 65536> buf;
    std::string allBins;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(findCmd.c_str(), "r"), pclose);
    if (pipe) {
        while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr)
            allBins += buf.data();
    }

    if (allBins.empty()) return "";

    std::vector<std::string> bins;
    std::stringstream ss(allBins);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) bins.push_back(line);
    }

    std::string pkgShort = pkgname;
    auto dash = pkgShort.rfind('-');
    if (dash != std::string::npos) pkgShort = pkgShort.substr(dash + 1);

    for (auto &b : bins) {
        auto slash = b.rfind('/');
        std::string fname = (slash != std::string::npos) ? b.substr(slash + 1) : b;
        if (fname == pkgname || fname == pkgShort) return b;
    }

    if (!bins.empty()) return bins[0];
    return "";
}

std::string Runner::detectInterpreter(const std::string &binaryPath) {
    std::ifstream ifs(binaryPath);
    std::string line;
    if (std::getline(ifs, line) && line.size() > 2 && line.substr(0, 2) == "#!") {
        std::string rest = line.substr(2);
        size_t start = rest.find_first_not_of(" \t");
        if (start != std::string::npos) rest = rest.substr(start);
        if (rest.find("/usr/bin/env ") == 0 || rest.find("/bin/env ") == 0) {
            size_t space = rest.rfind(' ');
            if (space != std::string::npos) return rest.substr(space + 1);
        }
        size_t slash = rest.rfind('/');
        if (slash != std::string::npos) return rest.substr(slash + 1);
        return rest;
    }
    return "";
}

std::string Runner::findSystemInterpreter(const std::string &name) {
    const char *userC = getenv("USER");
    std::string user = userC ? userC : "";
    std::vector<std::string> paths;
    paths.push_back("/run/current-system/sw/bin/" + name);
    paths.push_back("/run/wrappers/bin/" + name);
    if (!user.empty())
        paths.push_back("/etc/profiles/per-user/" + user + "/bin/" + name);
    const char *home = getenv("HOME");
    if (home) {
        paths.push_back(std::string(home) + "/.nix-profile/bin/" + name);
    }

    for (auto &p : paths) {
        std::string check = "test -x " + shq(p) + " 2>/dev/null";
        if (std::system(check.c_str()) == 0) return p;
    }

    std::string check = "command -v " + shq(name) + " 2>/dev/null";
    std::array<char, 4096> buf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(check.c_str(), "r"), pclose);
    if (pipe) {
        if (fgets(buf.data(), buf.size(), pipe.get()) != nullptr)
            result = buf.data();
    }
    if (!result.empty()) {
        if (result.back() == '\n') result.pop_back();
        return result;
    }

    // Try nix-shell
    std::string nixCheck = "nix-shell -p " + shq(name)
        + " --run 'type -P " + shq(name) + "' 2>/dev/null";
    std::unique_ptr<FILE, decltype(&pclose)> pipe2(popen(nixCheck.c_str(), "r"), pclose);
    if (pipe2) {
        std::array<char, 4096> buf2;
        std::string result2;
        while (fgets(buf2.data(), buf2.size(), pipe2.get()) != nullptr)
            result2 += buf2.data();
        if (!result2.empty()) {
            if (result2.back() == '\n') result2.pop_back();
            return result2;
        }
    }

    return "";
}

static std::string queryLibDirs(const std::string &base, const std::string &pattern) {
    // Quote base path but NOT the glob — shell needs to expand wildcards
    bool hasGlob = pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
    std::string cmd;
    if (hasGlob)
        cmd = "ls -d " + shq(base + "/") + pattern + " 2>/dev/null";
    else
        cmd = "ls -d " + shq(base + "/" + pattern) + " 2>/dev/null";
    std::array<char, 4096> buf;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (pipe) {
        while (fgets(buf.data(), buf.size(), pipe.get()) != nullptr)
            result += buf.data();
    }
    // Convert newlines to colons for PATH-like env vars
    for (auto &c : result) if (c == '\n') c = ':';
    if (!result.empty() && result.back() == ':') result.pop_back();
    return result;
}

// For interpreted scripts: PATH + language-specific vars only (no LD_LIBRARY_PATH)
static std::string langEnvPrefix(const std::string &interp, const std::string &depsPrefix) {
    std::string path = depsPrefix + "/usr/bin:" + depsPrefix + "/usr/lib/go/bin:" + depsPrefix + "/usr/local/bin";
    std::string env = "PATH=" + shq(path) + ":$PATH ";

    if (interp == "ruby") {
        std::string gemsDir = queryLibDirs(depsPrefix + "/usr/lib/ruby/gems", "*");
        if (!gemsDir.empty())
            env += "GEM_PATH=" + shq(gemsDir) + " ";
    } else if (interp == "python" || interp == "python3") {
        std::string pyLibs = queryLibDirs(depsPrefix + "/usr/lib", "python*/site-packages");
        if (!pyLibs.empty())
            env += "PYTHONPATH=" + shq(pyLibs) + ":$PYTHONPATH ";
    } else if (interp == "perl") {
        std::string perlLibs = queryLibDirs(depsPrefix + "/usr/lib/perl5", "*");
        if (!perlLibs.empty())
            env += "PERL5LIB=" + shq(perlLibs) + ":$PERL5LIB ";
    }

    return env;
}

// For ELF binaries: PATH + LD_LIBRARY_PATH pointing to Arch system-deps
static std::string elfEnvPrefix(const std::string &depsPrefix) {
    std::string path = depsPrefix + "/usr/bin:" + depsPrefix + "/usr/lib/go/bin:" + depsPrefix + "/usr/local/bin";
    std::string libPath = depsPrefix + "/usr/lib:" + depsPrefix + "/usr/lib/x86_64-linux-gnu";
    return "PATH=" + shq(path) + ":$PATH LD_LIBRARY_PATH=" + shq(libPath) + ":$LD_LIBRARY_PATH ";
}

bool Runner::runWithInterpreter(const std::string &interp,
                                 const std::string &scriptPath,
                                 const std::vector<std::string> &args) {
    std::cout << "  interpreter: " << interp << "\n";

    // Try to find the system interpreter (NixOS-compatible)
    std::string interpPath = findSystemInterpreter(interp);

    if (interpPath.empty()) {
        std::cerr << "  " << interp << " not found on system — "
                  << "will try via nix-shell\n";
        // Use nix-shell to get the interpreter at runtime
        std::string innerCmd = langEnvPrefix(interp, depsPrefix()) + shq(interp) + " " + shq(scriptPath);
        for (auto &a : args) innerCmd += " " + shq(a);
        std::string nixCmd = "nix-shell -p " + shq(interp) + " --run " + shq(innerCmd);
        std::cout << "  running via nix-shell...\n";
        int ret = std::system(nixCmd.c_str());
        if (ret != 0) {
            std::cerr << "error: " << interp << " execution failed\n";
            return false;
        }
        return true;
    }

    std::cout << "  using system: " << interpPath << "\n";

    // Use language-specific env (PATH + GEM_PATH etc., NO LD_LIBRARY_PATH)
    std::string envPrefix = langEnvPrefix(interp, depsPrefix());

    std::string cmd = envPrefix + shq(interpPath) + " " + shq(scriptPath);
    for (auto &a : args) cmd += " " + shq(a);

    int ret = std::system(cmd.c_str());
    if (ret != 0 && WIFSIGNALED(ret)) {
        std::cerr << "error: " << interp << " killed by signal "
                  << WTERMSIG(ret) << "\n";
        return false;
    }
    return true;
}

bool Runner::runElfNative(const std::string &binaryPath,
                           const std::vector<std::string> &args) {
    std::string envPrefix = elfEnvPrefix(depsPrefix());

    std::string cmd = envPrefix + shq(binaryPath);
    for (auto &a : args) cmd += " " + shq(a);

    int ret = std::system(cmd.c_str());
    if (ret == 0) return true;

    std::cout << "  native ELF failed (expected on NixOS)\n";
    return false;
}

bool Runner::runElfSteam(const std::string &binaryPath,
                          const std::vector<std::string> &args) {
    std::string envPrefix = elfEnvPrefix(depsPrefix());

    std::string cmd = envPrefix + "steam-run " + shq(binaryPath);
    for (auto &a : args) cmd += " " + shq(a);

    std::cout << "  retrying via steam-run...\n";
    int ret = std::system(cmd.c_str());

    if (ret == 0) return true;

    std::cerr << "error: steam-run also failed (exit code "
              << WEXITSTATUS(ret) << ")\n";

    // Last resort: try nix-shell FHS approach
    std::cout << "  trying nix-shell FHS fallback...\n";
    std::string nixCmd = "nix-shell -p steam-run --run " + shq(cmd);
    ret = std::system(nixCmd.c_str());
    if (ret == 0) return true;

    std::cerr << "error: all methods failed to run binary\n";
    return false;
}

bool Runner::runPackage(const std::string &pkgname,
                        const std::vector<std::string> &args) {
    std::cout << "== Running " << pkgname << " in sandbox ==\n";
    setupSandbox(pkgname);
    std::string sandbox = sandboxDir(pkgname);

    // Find the binary
    std::string binary = findBinary(pkgname);
    if (binary.empty()) {
        std::cerr << "error: no executable found for " << pkgname << "\n";
        return false;
    }

    std::cout << "  binary: " << binary << "\n";

    // Detect if it's an interpreted script
    std::string interp = detectInterpreter(binary);

    // Set up sandbox environment (don't change HOME — breaks path resolution)
    setenv("TMPDIR", (sandbox + "/tmp").c_str(), 1);
    setenv("XDG_CACHE_HOME", (sandbox + "/tmp/cache").c_str(), 1);
    setenv("XDG_CONFIG_HOME", (sandbox + "/config").c_str(), 1);
    setenv("XDG_DATA_HOME", (sandbox + "/data").c_str(), 1);

    bool result = false;
    if (!interp.empty()) {
        result = runWithInterpreter(interp, binary, args);
    } else {
        result = runElfNative(binary, args);
        if (!result) result = runElfSteam(binary, args);
    }

    // Don't auto-clean so users can inspect failures
    return result;
}
