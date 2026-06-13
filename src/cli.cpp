#include "cli.h"
#include "aur.h"
#include "pkgbuild.h"
#include "resolver.h"
#include "craftr.h"
#include <iostream>
#include <algorithm>

static const char *VERSION = "0.1.0";

CliOptions Cli::parseArgs(int argc, char *argv[]) {
    CliOptions opts;
    bool optionsDone = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--") { optionsDone = true; continue; }
        if (!optionsDone && arg[0] == '-' && arg.size() > 1) {
            if (opts.run) {
                opts.targets.push_back(arg);
                continue;
            }
            for (size_t j = 1; j < arg.size(); j++) {
                switch (arg[j]) {
                    case 'S': opts.sync = true;         break;
                    case 'y': opts.refresh = true;      break;
                    case 'u': opts.sysupgrade = true;   break;
                    case 's': opts.search = true;        break;
                    case 'i': opts.info = true;          break;
                    case 'R': opts.remove = true;        break;
                    case 'Q': opts.query = true;         break;
                    case 'r': opts.run = true;           break;
                    case 'P': opts.prepareOnly = true;   break;
                    case 'h': opts.help = true;          break;
                    case 'V': opts.version = true;       break;
                    default:
                        std::cerr << "error: unknown option -" << arg[j] << "\n";
                        opts.help = true;
                        return opts;
                }
            }
        } else {
            opts.targets.push_back(arg);
        }
    }
    return opts;
}

int Cli::run(int argc, char *argv[]) {
    auto opts = parseArgs(argc, argv);

    if (opts.help || argc == 1) {
        printUsage();
        return 0;
    }
    if (opts.version) {
        printVersion();
        return 0;
    }
    if (opts.run)    return cmdRun(opts);
    if (opts.remove) return cmdRemove(opts);
    if (opts.query)  return cmdQuery(opts);
    if (opts.sync && opts.search) return cmdSearch(opts);
    if (opts.sync && opts.info)   return cmdInfo(opts);
    if (opts.sync)                return cmdSync(opts);

    std::cerr << "error: no operation specified (use -h for help)\n";
    return 1;
}

int Cli::cmdSync(const CliOptions &opts) {
    std::cout << "== nixtaur sync ==" << std::endl;
    Aur aur;
    if (opts.refresh) {
        std::cout << "  -> refreshing AUR database...\n";
        aur.refresh();
    }
    if (opts.sysupgrade) {
        std::cout << "  -> checking for upgrades...\n";
        auto outdated = aur.checkUpgrades({});
        for (auto &pkg : outdated)
            std::cout << "  -> " << pkg << " needs upgrade\n";
        if (outdated.empty())
            std::cout << "  -> all packages up to date\n";
    }
    if (opts.targets.empty()) {
        if (!opts.sysupgrade)
            std::cout << "  -> no targets specified\n";
        return 0;
    }

    std::cout << "  targets:";
    for (auto &t : opts.targets) std::cout << " " << t;
    std::cout << "\n";

    std::cout << "== Resolving dependencies ==\n";
    Resolver resolver;
    auto order = resolver.resolve(opts.targets);

    if (order.empty() && resolver.systemDeps().empty()) {
        std::cout << "  nothing to do.\n";
        return 0;
    }

    Craftr craftr;

    if (!resolver.systemDeps().empty()) {
        std::cout << "\n== Downloading system dependencies from Arch repos ==\n";
        if (!craftr.downloadSystemDeps(resolver.systemDeps())) {
            std::cerr << "FAILED to download system dependencies\n";
            return 1;
        }
    }

    for (auto &pkg : order) {
        std::cout << "\n########################################\n";
        std::cout << "# Package: " << pkg << "\n";
        std::cout << "########################################\n";

        if (!craftr.prepare(pkg)) {
            std::cerr << "FAILED to prepare " << pkg << "\n";
            return 1;
        }
        if (opts.prepareOnly) {
            std::cout << "  -> " << pkg << " prepared (skipped build due to -P)\n";
            continue;
        }
        if (!craftr.build(pkg)) {
            std::cerr << "FAILED to build " << pkg << "\n";
            return 1;
        }
        craftr.install(pkg);
    }

    std::cout << "\n== All done ==\n";
    return 0;
}

int Cli::cmdSearch(const CliOptions &opts) {
    Aur aur;
    for (auto &target : opts.targets) {
        std::cout << "searching AUR for \"" << target << "\"...\n";
        auto results = aur.search(target);
        if (results.empty()) {
            std::cout << "  no results found\n";
            continue;
        }
        for (auto &pkg : results) {
            if (isBlockedPackage(pkg.name)) continue;
            std::cout << "aur/" << pkg.name << " " << pkg.version
                      << "  (" << pkg.description << ")\n";
        }
    }
    return 0;
}

int Cli::cmdInfo(const CliOptions &opts) {
    Aur aur;
    for (auto &target : opts.targets) {
        if (isBlockedPackage(target)) {
            std::cout << target << " is blocked (kernel/driver)\n";
            continue;
        }
        std::cout << "fetching info for \"" << target << "\"...\n";
        auto info = aur.getInfo(target);
        if (info.name.empty()) {
            std::cerr << "  not found in AUR\n";
            continue;
        }
        std::cout << "Repository  : aur\n"
                  << "Name        : " << info.name << "\n"
                  << "Version     : " << info.version << "\n"
                  << "Description : " << info.description << "\n"
                  << "URL         : " << info.url << "\n"
                  << "License     : ";
        for (auto &lic : info.license) std::cout << lic << " ";
        std::cout << "\nDepends On  : ";
        for (auto &dep : info.depends) std::cout << dep << " ";
        std::cout << "\nMake Deps   : ";
        for (auto &dep : info.makedepends) std::cout << dep << " ";
        std::cout << "\n";
    }
    return 0;
}

int Cli::cmdRemove(const CliOptions &) {
    std::cerr << "error: remove not yet implemented\n";
    return 1;
}

int Cli::cmdQuery(const CliOptions &) {
    std::cerr << "error: query not yet implemented\n";
    return 1;
}

int Cli::cmdRun(const CliOptions &opts) {
    if (opts.targets.empty()) {
        std::cerr << "error: specify a package to run\n";
        return 1;
    }
    Craftr craftr;
    std::vector<std::string> args;
    if (opts.targets.size() > 1)
        args.assign(opts.targets.begin() + 1, opts.targets.end());
    if (!craftr.run(opts.targets[0], args)) {
        std::cerr << "error: failed to run " << opts.targets[0] << "\n";
        return 1;
    }
    return 0;
}

void Cli::printUsage() {
    std::cout << "nixtaur " << VERSION
              << " — AUR translation layer for NixOS\n\n"
              << "Usage:\n"
              << "  nixtaur -S <pkg>         Build & install AUR package\n"
              << "  nixtaur -Ss <keyword>    Search AUR\n"
              << "  nixtaur -Si <pkg>        Package info\n"
              << "  nixtaur -Sy              Refresh AUR database\n"
              << "  nixtaur -Su              Upgrade installed AUR packages\n"
              << "  nixtaur -SP <pkg>        Prepare only (download + extract, no build)\n"
              << "  nixtaur -r <pkg> [args]  Run installed pkg via steam-run\n"
              << "  nixtaur -h               Help\n"
              << "  nixtaur -V               Version\n\n"
              << "Notes:\n"
              << "  - Built packages go to ~/.local/share/nixtaur/install/\n"
              << "  - Uses steam-run for FHS compatibility on NixOS\n"
              << "  - Place patches in ~/.config/nixtaur/patches/<pkgname>/\n"
              << "  - Kernel/driver packages are blocked by default\n";
}

void Cli::printVersion() {
    std::cout << "nixtaur " << VERSION << "\n";
}
