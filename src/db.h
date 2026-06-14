#pragma once
#include <string>
#include <vector>

struct InstalledPkg {
    std::string name;
    std::string version;
    std::string type; // "aur" or "arch"
};

class Db {
public:
    Db();
    void add(const std::string &name, const std::string &version, const std::string &type);
    void remove(const std::string &name);
    std::vector<InstalledPkg> list() const;
    bool isInstalled(const std::string &name) const;
    std::string getVersion(const std::string &name) const;

private:
    std::string dbPath() const;
    std::string dirPath() const;
    std::vector<InstalledPkg> load() const;
    void save(const std::vector<InstalledPkg> &pkgs) const;
};
