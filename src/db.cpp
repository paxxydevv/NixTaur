#include "db.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

static std::string dataDir() {
    const char *home = getenv("HOME");
    return (home ? std::string(home) : "/tmp") + "/.local/share/nixtaur";
}

std::string Db::dirPath() const { return dataDir(); }
std::string Db::dbPath() const { return dataDir() + "/installed-pkgs.txt"; }

Db::Db() {}

std::vector<InstalledPkg> Db::load() const {
    std::vector<InstalledPkg> pkgs;
    std::ifstream ifs(dbPath());
    if (!ifs.is_open()) return pkgs;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string name, version, type;
        std::getline(ss, name, ':');
        std::getline(ss, version, ':');
        std::getline(ss, type, ':');
        if (!name.empty()) pkgs.push_back({name, version, type});
    }
    return pkgs;
}

void Db::save(const std::vector<InstalledPkg> &pkgs) const {
    std::system(("mkdir -p " + dirPath()).c_str());
    std::ofstream ofs(dbPath());
    if (!ofs.is_open()) {
        std::cerr << "  warning: could not write package database\n";
        return;
    }
    for (auto &p : pkgs)
        ofs << p.name << ":" << p.version << ":" << p.type << "\n";
}

void Db::add(const std::string &name, const std::string &version, const std::string &type) {
    auto pkgs = load();
    for (auto &p : pkgs) {
        if (p.name == name) {
            p.version = version;
            p.type = type;
            save(pkgs);
            return;
        }
    }
    pkgs.push_back({name, version, type});
    save(pkgs);
}

void Db::remove(const std::string &name) {
    auto pkgs = load();
    pkgs.erase(std::remove_if(pkgs.begin(), pkgs.end(),
        [&](const InstalledPkg &p) { return p.name == name; }), pkgs.end());
    save(pkgs);
}

std::vector<InstalledPkg> Db::list() const {
    return load();
}

bool Db::isInstalled(const std::string &name) const {
    auto pkgs = load();
    return std::find_if(pkgs.begin(), pkgs.end(),
        [&](const InstalledPkg &p) { return p.name == name; }) != pkgs.end();
}

std::string Db::getVersion(const std::string &name) const {
    auto pkgs = load();
    for (auto &p : pkgs)
        if (p.name == name) return p.version;
    return "";
}
