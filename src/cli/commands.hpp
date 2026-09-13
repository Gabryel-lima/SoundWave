#pragma once

#include <string>
#include <vector>

namespace soundwave::cli {

// Argumentos ja separados em posicionais e opcoes --chave=valor / --chave valor.
struct Args {
    std::vector<std::string> positional;
    std::vector<std::pair<std::string, std::string>> options;

    [[nodiscard]] bool has(const std::string& key) const;
    [[nodiscard]] std::string get(const std::string& key, const std::string& fallback = {}) const;
    [[nodiscard]] double getDouble(const std::string& key, double fallback) const;
    [[nodiscard]] std::size_t getSize(const std::string& key, std::size_t fallback) const;
};

[[nodiscard]] Args parseArgs(int argc, char** argv);

int commandAnalyze(const Args& args);
int commandCompare(const Args& args);
int commandMap(const Args& args);
int commandGenerate(const Args& args);
int commandConfig(const Args& args);
int commandInfo(const Args& args);
int commandLive(const Args& args);

void printUsage();

}  // namespace soundwave::cli
