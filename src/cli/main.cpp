#include <exception>
#include <iostream>
#include <string>

#include "commands.hpp"

int main(int argc, char** argv) {
    using namespace soundwave::cli;

    if (argc < 2) {
        printUsage();
        return 1;
    }

    const Args args = parseArgs(argc, argv);
    const std::string command = args.positional.empty() ? std::string{} : args.positional.front();

    try {
        if (command == "analyze") return commandAnalyze(args);
        if (command == "compare") return commandCompare(args);
        if (command == "map") return commandMap(args);
        if (command == "gen" || command == "generate") return commandGenerate(args);
        if (command == "config") return commandConfig(args);
        if (command == "info") return commandInfo(args);
        if (command == "medium") return commandMedium(args);
        if (command == "fidelity") return commandFidelity(args);
        if (command == "live") return commandLive(args);
        if (command == "help" || command == "--help" || command == "-h") {
            printUsage();
            return 0;
        }
    } catch (const std::exception& error) {
        // Erros de configuracao chegam aqui como excecao dos construtores.
        // Melhor uma mensagem legivel do que um terminate() sem contexto.
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }

    std::cerr << "comando desconhecido: " << command << "\n\n";
    printUsage();
    return 1;
}
