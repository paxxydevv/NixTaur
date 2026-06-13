#pragma once
#include <string>
#include <vector>

struct CliOptions {
    bool sync       = false;
    bool refresh    = false;
    bool sysupgrade = false;
    bool search     = false;
    bool info       = false;
    bool remove     = false;
    bool query      = false;
    bool run        = false;
    bool help       = false;
    bool version    = false;
    bool prepareOnly = false;
    std::vector<std::string> targets;
};

class Cli {
public:
    int run(int argc, char *argv[]);

private:
    CliOptions parseArgs(int argc, char *argv[]);
    int  cmdSync(const CliOptions &opts);
    int  cmdSearch(const CliOptions &opts);
    int  cmdInfo(const CliOptions &opts);
    int  cmdRemove(const CliOptions &opts);
    int  cmdQuery(const CliOptions &opts);
    int  cmdRun(const CliOptions &opts);
    void printUsage();
    void printVersion();
};
