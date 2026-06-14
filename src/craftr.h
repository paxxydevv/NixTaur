#pragma once
#include <string>
#include <vector>
#include <set>
#include "runner.h"

class Craftr {
public:
    bool prepare(const std::string &pkgname);
    bool build(const std::string &pkgname);
    bool install(const std::string &pkgname);
    bool run(const std::string &pkgname, const std::vector<std::string> &args);
    bool clean(const std::string &pkgname);
    bool downloadSystemDeps(const std::set<std::string> &deps);
    bool installSystemPackage(const std::string &pkgname);
    bool isInstalled(const std::string &pkgname) const;
    std::string workDir(const std::string &pkgname) const;
    std::string installDir(const std::string &pkgname) const;

private:
    std::string depsPrefix() const;
    bool verifyChecksums(const std::string &dir, const std::vector<std::string> &sources, const std::vector<std::string> &sha256sums) const;
    bool applyPatches(const std::string &srcdir, const std::string &pkgname) const;
    bool downloadSources(const std::string &dir, const std::vector<std::string> &sources) const;
    bool generateStubs(const std::string &dir, const std::vector<std::string> &deps) const;
};
