#include "aur.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <sstream>
#include <algorithm>
#include <iostream>

std::string Aur::httpGet(const std::string &url) const {
    std::cerr << "  [aur] fetching: " << url << "\n";
    std::string cmd = "curl -sL --proto =https --connect-timeout 30 --max-time 15 '" + url + "' 2>&1";
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        std::cerr << "  [aur] error: failed to run curl\n";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    if (result.empty())
        std::cerr << "  [aur] warning: empty response from AUR\n";
    return result;
}

static std::string dejsn(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i+1]) {
                case 'n':  out += '\n'; i++; break;
                case 't':  out += '\t'; i++; break;
                case 'r':  out += '\r'; i++; break;
                case '\\': out += '\\'; i++; break;
                case '"':  out += '"';  i++; break;
                case 'u': {
                    if (i + 5 < s.size()) {
                        std::string hex = s.substr(i + 2, 4);
                        unsigned long cp = std::strtoul(hex.c_str(), nullptr, 16);
                        if (cp <= 0x7F) {
                            out += (char)cp;
                        } else if (cp <= 0x7FF) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                        i += 5;
                    }
                    break;
                }
                default: out += s[i+1]; i++; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::string grabStr(const std::string &json, const std::string &key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find_first_of("\"", pos + 1);
    if (pos == std::string::npos) return "";
    auto start = pos + 1;
    auto end = json.find("\"", start);
    if (end == std::string::npos) return "";
    return dejsn(json.substr(start, end - start));
}

static std::vector<std::string> grabArr(const std::string &json, const std::string &key) {
    std::vector<std::string> result;
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    pos = json.find('[', pos);
    if (pos == std::string::npos) return result;
    auto end = json.find(']', pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);
    size_t start = 0;
    while ((start = arr.find('"', start)) != std::string::npos) {
        auto e = arr.find('"', start + 1);
        if (e == std::string::npos) break;
        result.push_back(dejsn(arr.substr(start + 1, e - start - 1)));
        start = e + 1;
    }
    return result;
}

static std::vector<AurPkgInfo> parseResults(const std::string &json) {
    std::vector<AurPkgInfo> results;

    auto extractOne = [&](const std::string &obj) -> AurPkgInfo {
        AurPkgInfo info;
        info.name         = grabStr(obj, "Name");
        info.version      = grabStr(obj, "Version");
        info.description  = grabStr(obj, "Description");
        info.url          = grabStr(obj, "URL");
        info.license      = grabArr(obj, "License");
        info.depends      = grabArr(obj, "Depends");
        info.makedepends  = grabArr(obj, "MakeDepends");
        info.checkdepends = grabArr(obj, "CheckDepends");
        return info;
    };

    size_t p = 0;
    while ((p = json.find("{\"ID\"", p)) != std::string::npos) {
        auto end = json.find('}', p);
        if (end == std::string::npos) break;
        std::string obj = json.substr(p, end - p + 1);
        auto info = extractOne(obj);
        if (!info.name.empty()) results.push_back(info);
        p = end + 1;
    }

    if (results.empty()) {
        auto info = extractOne(json);
        if (!info.name.empty()) results.push_back(info);
    }

    std::cerr << "  [aur] parsed " << results.size() << " result(s)\n";
    return results;
}

std::vector<AurPkgInfo> Aur::search(const std::string &query) {
    std::cerr << "  [aur] searching AUR for \"" << query << "\"...\n";
    std::string url = std::string(RPC_URL) + "/search/" + query;
    auto json = httpGet(url);
    if (json.empty()) return {};
    return parseResults(json);
}

AurPkgInfo Aur::getInfo(const std::string &pkgname) {
    std::cerr << "  [aur] fetching info for \"" << pkgname << "\"...\n";
    std::string url = std::string(RPC_URL) + "/info/" + pkgname;
    auto json = httpGet(url);
    if (json.empty()) {
        std::cerr << "  [aur] empty response, package likely not in AUR\n";
        return {};
    }

    auto results = parseResults(json);
    if (results.empty()) {
        std::cerr << "  [aur] no results parsed — \"" << pkgname
                  << "\" is not in the AUR (may be in official Arch repos)\n";
        return {};
    }

    std::cerr << "  [aur] found: " << results[0].name
              << " v" << results[0].version << "\n";
    return results[0];
}

bool Aur::downloadSource(const std::string &pkgname) {
    std::string url = "https://aur.archlinux.org/cgit/aur.git/snapshot/"
                      + pkgname + ".tar.gz";
    std::cerr << "  [aur] downloading PKGBUILD snapshot: " << url << "\n";
    std::string cmd = "curl -sL --proto =https '" + url + "' 2>/dev/null | tar -xz 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "  [aur] failed to download/extract snapshot\n";
        return false;
    }
    std::cerr << "  [aur] snapshot extracted to ./" << pkgname << "/\n";
    return true;
}

bool Aur::refresh() {
    return true;
}

std::vector<std::string> Aur::checkUpgrades(const std::map<std::string, std::string> &installed) {
    if (installed.empty()) return {};

    std::string names;
    for (auto &[name, ver] : installed) {
        if (!names.empty()) names += ",";
        names += name;
    }

    std::string url = std::string(RPC_URL) + "/info/" + names;
    auto json = httpGet(url);
    if (json.empty()) {
        std::cerr << "  [aur] upgrade check: empty response from AUR\n";
        return {};
    }

    auto results = parseResults(json);
    std::vector<std::string> outdated;
    for (auto &pkg : results) {
        auto it = installed.find(pkg.name);
        if (it == installed.end()) continue;
        if (it->second != pkg.version) {
            std::cout << "  [aur] " << pkg.name << ": " << it->second
                      << " -> " << pkg.version << "\n";
            outdated.push_back(pkg.name);
        }
    }

    if (outdated.empty())
        std::cout << "  [aur] all packages up to date\n";
    return outdated;
}
