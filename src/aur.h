#pragma once
#include <string>
#include <vector>
#include <map>

struct AurPkgInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string url;
    std::vector<std::string> license;
    std::vector<std::string> depends;
    std::vector<std::string> makedepends;
    std::vector<std::string> checkdepends;
};

class Aur {
public:
    std::vector<AurPkgInfo> search(const std::string &query);
    AurPkgInfo              getInfo(const std::string &pkgname);
    bool                    downloadSource(const std::string &pkgname);
    bool                    refresh();
    std::vector<std::string> checkUpgrades(const std::map<std::string, std::string> &installed);

private:
    static constexpr const char *RPC_URL = "https://aur.archlinux.org/rpc/v5";
    std::string httpGet(const std::string &url) const;
};
