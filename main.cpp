#include "assembler.h"

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

void printErrors(const std::vector<AssemblyError>& errors) {
    for (const AssemblyError& error : errors) {
        std::cout << "ERRO " << error.type << " (linha " << error.lineNumber
                  << "): " << error.message << '\n';
    }
}
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Uso: montador program.asm\n";
        return 1;
    }

    std::vector<AssemblyError> errors;
    const ObjectModule object = assemble(argv[1], errors);
    if (!errors.empty()) {
        printErrors(errors);
        return 1;
    }

    const std::filesystem::path outputPath =
        std::filesystem::path(argv[1]).replace_extension(OBJ_EXT);
    std::ofstream output(outputPath);
    if (!output) {
        std::cout << "ERRO SEMANTICO (linha 1): nao foi possivel criar '" <<
                     outputPath.string() << "'\n";
        return 1;
    }
    if (object.isModule) {
        for (const auto& definition : object.defs) {
            output << "D, " << definition.first << ' ' << definition.second << '\n';
        }
        for (const auto& use : object.uses) {
            output << "U, " << use.first << ' ' << use.second << '\n';
        }
        output << "R, ";
        for (std::size_t i = 0; i < object.reloc.size(); ++i) {
            if (i != 0) {
                output << ' ';
            }
            output << (object.reloc[i] ? 1 : 0);
        }
        output << '\n';
    }
    output << codeLine(object.code);
    if (!output) {
        std::cout << "ERRO SEMANTICO (linha 1): falha ao gravar arquivo de saida\n";
        return 1;
    }
    return 0;
}
