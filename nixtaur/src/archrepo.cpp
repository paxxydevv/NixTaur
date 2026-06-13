#include "archrepo.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <thread>
#include <chrono>

static std::map<std::string, ArchPkgInfo> searchCache;

std::string ArchRepo::httpGet(const std::string &url) const {
    std::string cmd = "curl -sL --connect-timeout 15 --max-time 15 '" + url + "' 2>/dev/null";
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();
    return result;
}

static std::string grabStr(const std::string &json, const std::string &key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find_first_of("\"", pos + 1);
    if (pos == std::string::npos) return "";
    auto start = pos + 1;
    auto end = json.find("\"", start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
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
        result.push_back(arr.substr(start + 1, e - start - 1));
        start = e + 1;
    }
    return result;
}

static std::string mapPkgname(const std::string &name) {
    static const std::map<std::string, std::string> aliases = {
        {"sh", "bash"},
        {"libgcc", "gcc-libs"},
        {"libstdc++", "gcc-libs"},
        {"lz4", "lz4"},
        {"libcrypto.so", "openssl"},
        {"libssl.so", "openssl"},
        {"libz.so", "zlib"},
        {"libbz2.so", "bzip2"},
        {"liblzma.so", "xz"},
        {"libzstd.so", "zstd"},
    };
    auto it = aliases.find(name);
    if (it != aliases.end()) return it->second;
    return name;
}

ArchPkgInfo ArchRepo::search(const std::string &rawName) {
    std::string pkgname = mapPkgname(rawName);

    {
        auto it = searchCache.find(pkgname);
        if (it != searchCache.end()) return it->second;
    }

    ArchPkgInfo info;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    std::string url = std::string(SEARCH_URL) + pkgname;
    std::string json = httpGet(url);
    if (json.empty()) {
        searchCache[pkgname] = info;
        return info;
    }

    auto resultsPos = json.find("\"results\"");
    if (resultsPos == std::string::npos) {
        searchCache[pkgname] = info;
        return info;
    }

    auto objStart = json.find('{', resultsPos);
    if (objStart == std::string::npos) {
        searchCache[pkgname] = info;
        return info;
    }

    auto objEnd = json.find('}', objStart);
    if (objEnd == std::string::npos) {
        searchCache[pkgname] = info;
        return info;
    }

    std::string obj = json.substr(objStart, objEnd - objStart + 1);
    std::string name = grabStr(obj, "pkgname");

    if (name != pkgname) {
        size_t pos = objEnd;
        while ((pos = json.find('{', pos + 1)) != std::string::npos) {
            objEnd = json.find('}', pos);
            if (objEnd == std::string::npos) break;
            obj = json.substr(pos, objEnd - pos + 1);
            name = grabStr(obj, "pkgname");
            if (name == pkgname) break;
            pos = objEnd;
        }
        if (name != pkgname) {
            searchCache[pkgname] = info;
            return info;
        }
    }

    info.found = true;
    info.pkgname = grabStr(obj, "pkgname");
    info.repo = grabStr(obj, "repo");
    std::string ver = grabStr(obj, "pkgver");
    std::string rel = grabStr(obj, "pkgrel");
    info.version = ver + "-" + rel;
    info.filename = grabStr(obj, "filename");
    info.depends = grabArr(obj, "depends");
    info.makedepends = grabArr(obj, "makedepends");

    if (info.filename.empty())
        info.filename = info.pkgname + "-" + info.version + "-x86_64.pkg.tar.zst";

    searchCache[pkgname] = info;
    return info;
}

bool ArchRepo::downloadAndExtract(const std::string &pkgname, const std::string &destdir) {
    auto info = search(pkgname);
    if (!info.found) {
        std::cerr << "    [arch] package \"" << pkgname << "\" not found in official repos\n";
        return false;
    }

    std::string dldir = destdir + "/.cache";
    std::string mkdir = "mkdir -p '" + dldir + "'";
    std::system(mkdir.c_str());

    std::string url = std::string(ARCHIVE_URL) + info.filename;
    std::string localFile = dldir + "/" + info.filename;

    std::cout << "    [arch] downloading " << info.filename
              << " (" << info.repo << "/" << info.pkgname << ")\n";

    std::string dl = "curl -fsSL --connect-timeout 30 --max-time 120 -o '"
                     + localFile + "' '" + url + "' 2>&1";
    int ret = std::system(dl.c_str());
    if (ret != 0) {
        std::cerr << "    [arch] download failed: " << url << "\n";
        return false;
    }

    std::cout << "    [arch] extracting to " << destdir << "...\n";
    std::string extract = "tar --zstd -xf '" + localFile + "' -C '" + destdir
                          + "' 2>/dev/null";
    std::system(extract.c_str());

    std::string rm = "rm '" + localFile + "'";
    std::system(rm.c_str());

    return true;
}
