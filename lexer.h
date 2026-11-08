#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "structures.h"

// Estrutura para representar uma linha já tokenizada e sanitizada
struct LineData {
    int originalLineNumber;         // Número da linha no arquivo .asm original
    std::string label;              // Rótulo (se houver, sem o ':')
    std::string instruction;        // Instrução ou diretiva (ex: ADD, SPACE)
    std::vector<std::string> args;  // Operandos/argumentos
};

// Converte string para maiúsculas
std::string toUpper(std::string str);

// Valida se um rótulo atende às regras léxicas da especificação
bool isValidLabel(const std::string& label, std::string& errorMsg);

// Processa o arquivo .asm inteiro e retorna as linhas estruturadas e os erros léxicos encontrados
void parseFile(const std::string& filename, 
               std::vector<LineData>& lines, 
               std::vector<AssemblyError>& errors);

#endif