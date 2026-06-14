#include "pkgbuild.h"
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <iostream>

bool parsePkgbuild(const std::string &directory, Pkgbuild &out) {
    std::string getmeta = R"(__=$(
. ./PKGBUILD 2>/dev/null
echo "PKGNAME=$pkgname"
echo "PKGVER=$pkgver"
echo "PKGREL=$pkgrel"
echo "PKGDESC=$pkgdesc"
echo "URL=$url"
for i in "${license[@]}"; do echo "LICENSE=$i"; done
for i in "${depends[@]}"; do echo "DEPEND=$i"; done
for i in "${makedepends[@]}"; do echo "MAKEDEPEND=$i"; done
for i in "${checkdepends[@]}"; do echo "CHECKDEPEND=$i"; done
for i in "${conflicts[@]}"; do echo "CONFLICT=$i"; done
for i in "${provides[@]}"; do echo "PROVIDE=$i"; done
for i in "${source[@]}"; do echo "SOURCE=$i"; done
for i in "${sha256sums[@]}"; do echo "SHA256=$i"; done
echo "BUILD_START"
declare -f build 2>/dev/null || echo "build() { true; }"
echo "BUILD_END"
echo "PACKAGE_START"
declare -f package 2>/dev/null || echo "package() { true; }"
echo "PACKAGE_END"
) && echo "$__")";

    std::string cmd = "cd " + directory + " && " + getmeta;
    std::array<char, 4096> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return false;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    std::stringstream ss(result);
    std::string line;
    bool inBuild = false, inPackage = false;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line == "BUILD_START") { inBuild = true; inPackage = false; continue; }
        if (line == "BUILD_END")   { inBuild = false; continue; }
        if (line == "PACKAGE_START") { inPackage = true; inBuild = false; continue; }
        if (line == "PACKAGE_END")   { inPackage = false; continue; }

        if (inBuild) {
            out.buildFunction += line + "\n";
            continue;
        }
        if (inPackage) {
            out.packageFunction += line + "\n";
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "PKGNAME")  out.pkgname = val;
        else if (key == "PKGVER")  out.pkgver = val;
        else if (key == "PKGREL")  out.pkgrel = val;
        else if (key == "PKGDESC") out.pkgdesc = val;
        else if (key == "URL")     out.url = val;
        else if (key == "LICENSE")     out.license.push_back(val);
        else if (key == "DEPEND")      out.depends.push_back(val);
        else if (key == "MAKEDEPEND")  out.makedepends.push_back(val);
        else if (key == "CHECKDEPEND") out.checkdepends.push_back(val);
        else if (key == "CONFLICT")    out.conflicts.push_back(val);
        else if (key == "PROVIDE")     out.provides.push_back(val);
        else if (key == "SOURCE")      out.source.push_back(val);
        else if (key == "SHA256")      out.sha256sums.push_back(val);
    }

    return !out.pkgname.empty();
}

bool isBlockedPackage(const std::string &pkgname) {
    static const std::vector<std::string> blocked = {
        "linux",
        "linux-api-headers",
        "dkms",
    };
    for (auto &b : blocked) {
        if (pkgname == b) return true;
    }
    return false;
}
