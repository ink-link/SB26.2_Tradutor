#include <iostream>
#include <vector>
#include "structures.h"
#include "lexer.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Uso: ./montador <arquivo.asm>" << std::endl;
        return 1;
    }

    std::vector<LineData> parsedLines;
    std::vector<AssemblyError> errors;

    // Executa a leitura e sanitização
    parseFile(argv[1], parsedLines, errors);

    // Se houver erros léxicos/sintáticos básicos encontrados nessa fase:
    if (!errors.empty()) {
        std::cout << "--- ERROS ENCONTRADOS ---" << std::endl;
        for (const auto& err : errors) {
            std::cout << "Linha " << err.lineNumber << " [" << err.type << "]: " << err.message << std::endl;
        }
    } else {
        std::cout << "Arquivo processado e tokenizado com sucesso!\n" << std::endl;
        // Imprime o resultado para conferência do desenvolvedor
        for (const auto& line : parsedLines) {
            std::cout << "Linha " << line.originalLineNumber << " | ";
            if (!line.label.empty()) {
                std::cout << "Rotulo: " << line.label << " | ";
            }
            std::cout << "Inst/Dir: " << line.instruction << " | Args: ";
            for (const auto& arg : line.args) {
                std::cout << arg << " ";
            }
            std::cout << std::endl;
        }
    }

    return 0;
}