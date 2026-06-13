#include "resolver.h"
#include "aur.h"
#include "archrepo.h"
#include "pkgbuild.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <set>

static const std::set<std::string> STUBBED_DEPS = {
    "pacman",
    "makepkg",
};

static bool hasStub(const std::string &name) {
    return STUBBED_DEPS.count(name) > 0;
}

static bool isSonameDep(const std::string &name) {
    return name.size() > 3 &&
           (name.substr(name.size() - 3) == ".so" ||
            name.find(".so.") != std::string::npos);
}

static std::string cleanDepName(const std::string &raw) {
    std::string s = raw;
    auto pos = s.find_first_of("<>=");
    if (pos != std::string::npos) s = s.substr(0, pos);
    s = std::regex_replace(s, std::regex("^[^a-zA-Z0-9@_.+-]+"), "");
    s = std::regex_replace(s, std::regex("[^a-zA-Z0-9@_.+-]+$"), "");
    return s;
}

void Resolver::resolveOne(const std::string &pkgname, std::vector<std::string> &order) {
    if (visited_.count(pkgname)) return;
    visited_.insert(pkgname);

    if (isBlockedPackage(pkgname)) {
        std::cerr << "  [resolver] SKIPPED: " << pkgname
                  << " is blocked (kernel/driver)\n";
        return;
    }

    if (isSonameDep(pkgname)) {
        return;
    }

    std::cout << "  [resolver] checking \"" << pkgname << "\"...\n";

    Aur aur;
    auto info = aur.getInfo(pkgname);
    if (!info.name.empty()) {
        std::vector<std::string> allDeps;
        allDeps.insert(allDeps.end(), info.depends.begin(), info.depends.end());
        allDeps.insert(allDeps.end(), info.makedepends.begin(), info.makedepends.end());
        if (!allDeps.empty()) {
            std::cout << "    depends on: ";
            for (auto &d : allDeps) std::cout << d << " ";
            std::cout << "\n";
        }
        for (auto &dep : allDeps) {
            std::string depName = cleanDepName(dep);
            if (depName.empty()) continue;
            resolveOne(depName, order);
        }
        if (std::find(order.begin(), order.end(), pkgname) == order.end())
            order.push_back(pkgname);
        return;
    }

    if (hasStub(pkgname)) {
        std::cout << "  [resolver] STUBBED: \"" << pkgname
                  << "\" — nixtaur will provide a stub\n";
        stubbedDeps_.insert(pkgname);
        return;
    }

    std::cout << "    not in AUR — checking official Arch repos...\n";
    ArchRepo archRepo;
    auto archInfo = archRepo.search(pkgname);
    if (archInfo.found) {
        std::cout << "  [resolver] SYSTEM DEP: \"" << pkgname
                  << "\" found in " << archInfo.repo
                  << " (" << archInfo.version << ") — will download\n";
        systemDeps_.insert(pkgname);
        cachedInfo_[pkgname] = archInfo;

        for (auto &dep : archInfo.depends) {
            std::string depName = cleanDepName(dep);
            if (depName.empty()) continue;
            if (isBlockedPackage(depName)) continue;
            if (hasStub(depName)) continue;
            if (isSonameDep(depName)) continue;
            resolveOne(depName, order);
        }
        return;
    }

    std::cerr << "  [resolver] MISSING: \"" << pkgname
              << "\" not found in AUR or official Arch repos\n";
    missing_.insert(pkgname);
}

std::vector<std::string> Resolver::resolve(const std::vector<std::string> &targets) {
    std::cout << "== Resolver started ==\n";
    missing_.clear();
    visited_.clear();
    systemDeps_.clear();
    stubbedDeps_.clear();
    cachedInfo_.clear();

    std::vector<std::string> order;
    for (auto &t : targets) {
        std::cout << "[resolver] target: " << t << "\n";
        if (isBlockedPackage(t)) {
            std::cerr << "  [resolver] SKIPPED: " << t
                      << " is blocked (kernel/driver)\n";
            continue;
        }
        resolveOne(t, order);
    }

    if (!systemDeps_.empty()) {
        std::cout << "\n== System dependencies (will download from Arch repos) ==\n";
        for (auto &d : systemDeps_) std::cout << "  - " << d << "\n";
    }

    if (!stubbedDeps_.empty()) {
        std::cout << "== Stubbed dependencies ==\n";
        for (auto &d : stubbedDeps_) std::cout << "  - " << d << "\n";
    }

    if (!missing_.empty()) {
        std::cerr << "\n!! ERROR: Unresolved dependencies !!\n"
                  << "  The following packages were not found anywhere:\n";
        for (auto &m : missing_) {
            std::cerr << "    - " << m << "\n";
        }
        std::cerr << "  Aborting build.\n\n";
        return {};
    }

    if (order.empty() && systemDeps_.empty()) {
        std::cout << "  nothing to do.\n";
        return {};
    }

    std::cout << "== Build order ==\n";
    for (size_t i = 0; i < order.size(); i++) {
        std::cout << "  " << (i + 1) << ". " << order[i] << "\n";
    }
    std::cout << "=================\n";

    return order;
}

const ArchPkgInfo &Resolver::getCachedInfo(const std::string &name) const {
    static ArchPkgInfo empty;
    auto it = cachedInfo_.find(name);
    if (it != cachedInfo_.end()) return it->second;
    return empty;
}
