#pragma once
#include <string>
#include <vector>

struct Pkgbuild {
    std::string pkgname;
    std::string pkgver;
    std::string pkgrel;
    std::string pkgdesc;
    std::string url;
    std::vector<std::string> license;
    std::vector<std::string> depends;
    std::vector<std::string> makedepends;
    std::vector<std::string> checkdepends;
    std::vector<std::string> conflicts;
    std::vector<std::string> provides;
    std::vector<std::string> source;
    std::vector<std::string> sha256sums;
    std::string buildFunction;
    std::string packageFunction;
};

bool parsePkgbuild(const std::string &directory, Pkgbuild &out);
bool isBlockedPackage(const std::string &pkgname);
