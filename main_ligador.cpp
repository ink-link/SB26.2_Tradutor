#include "linker.h"

#include "structures.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
std::string codeLine(const std::vector<int>& words) {
    std::ostringstream output;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i != 0) {
            output << ' ';
        }
        output << words[i];
    }
    return output.str();
}

void printAssemblyErrors(const std::vector<AssemblyError>& errors) {
    for (const AssemblyError& error : errors) {
        std::cout << "ERRO " << error.type << " (linha " << error.lineNumber
                  << "): " << error.message << '\n';
    }
}
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cout << "Uso: ligador a.asm [b.asm [c.asm]]\n";
        return 1;
    }

    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        paths.emplace_back(argv[i]);
    }
    std::vector<int> executable;
    std::vector<AssemblyError> assemblyErrors;
    std::vector<std::string> linkErrors;
    if (!linkModules(paths, executable, assemblyErrors, linkErrors)) {
        printAssemblyErrors(assemblyErrors);
        for (const std::string& error : linkErrors) {
            std::cout << "ERRO SEMANTICO: " << error << '\n';
        }
        return 1;
    }

    const std::filesystem::path outputPath =
        std::filesystem::path(argv[1]).replace_extension(LIG_EXT);
    std::ofstream output(outputPath);
    if (!output) {
        std::cout << "ERRO SEMANTICO: nao foi possivel criar '" <<
                     outputPath.string() << "'\n";
        return 1;
    }
    output << codeLine(executable);
    if (!output) {
        std::cout << "ERRO SEMANTICO: falha ao gravar arquivo de saida\n";
        return 1;
    }
    return 0;
}
