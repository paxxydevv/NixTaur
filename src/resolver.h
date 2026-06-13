#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include "archrepo.h"

class Resolver {
public:
    std::vector<std::string> resolve(const std::vector<std::string> &targets);
    const std::set<std::string> &systemDeps() const { return systemDeps_; }
    const ArchPkgInfo &getCachedInfo(const std::string &name) const;

private:
    std::set<std::string> visited_;
    std::set<std::string> missing_;
    std::set<std::string> systemDeps_;
    std::set<std::string> stubbedDeps_;
    std::map<std::string, ArchPkgInfo> cachedInfo_;
    void resolveOne(const std::string &pkgname, std::vector<std::string> &order);
};
