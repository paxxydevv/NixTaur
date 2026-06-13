#pragma once
#include <string>
#include <vector>

struct ArchPkgInfo {
    std::string pkgname;
    std::string repo;
    std::string version;
    std::string filename;
    std::vector<std::string> depends;
    std::vector<std::string> makedepends;
    bool found = false;
};

class ArchRepo {
public:
    ArchPkgInfo search(const std::string &pkgname);
    bool downloadAndExtract(const std::string &pkgname, const std::string &destdir);

private:
    static constexpr const char *SEARCH_URL =
        "https://archlinux.org/packages/search/json/?name=";
    static constexpr const char *ARCHIVE_URL =
        "https://archive.archlinux.org/packages/.all/";
    std::string httpGet(const std::string &url) const;
};
