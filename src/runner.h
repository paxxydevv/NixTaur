#pragma once
#include <string>
#include <vector>

class Runner {
public:
    Runner();
    ~Runner();

    bool runPackage(const std::string &pkgname, const std::vector<std::string> &args);
    void cleanSandbox(const std::string &pkgname);

private:
    std::string sandboxRoot_;

    std::string depsPrefix() const;
    std::string installDir(const std::string &pkgname) const;
    std::string sandboxDir(const std::string &pkgname) const;

    std::string findBinary(const std::string &pkgname);
    std::string detectInterpreter(const std::string &binaryPath);
    std::string findSystemInterpreter(const std::string &name);

    bool runWithInterpreter(const std::string &interp,
                            const std::string &scriptPath,
                            const std::vector<std::string> &args);
    bool runElfNative(const std::string &binaryPath,
                      const std::vector<std::string> &args);
    bool runElfSteam(const std::string &binaryPath,
                     const std::vector<std::string> &args);

    bool setupSandbox(const std::string &pkgname);
};
