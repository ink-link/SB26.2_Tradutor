#include "lexer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// Remove tudo a partir do caractere ';'
static std::string stripComment(const std::string& line) {
    size_t pos = line.find(';');
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

// Validações Léxicas do Rótulo segundo o documento:
// 1. Limite de 30 caracteres
// 2. Composto por letras, números ou '_'
// 3. Primeiro caractere não pode ser um número
bool isValidLabel(const std::string& label, std::string& errorMsg) {
    if (label.empty()) {
        errorMsg = "Rótulo vazio";
        return false;
    }

    if (label.length() > 30) {
        errorMsg = "Rótulo '" + label + "' excede o limite de 30 caracteres";
        return false;
    }

    if (std::isdigit(static_cast<unsigned char>(label[0]))) {
        errorMsg = "Rótulo '" + label + "' não pode começar com um número";
        return false;
    }

    for (char c : label) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            errorMsg = "Caractere inválido '" + std::string(1, c) + "' no rótulo '" + label + "'";
            return false;
        }
    }

    return true;
}

void parseFile(const std::string& filename, 
               std::vector<LineData>& parsedLines, 
               std::vector<AssemblyError>& errors) {
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir o arquivo: " << filename << std::endl;
        return;
    }

    std::string currentLine;
    int lineNumber = 0;
    std::string pendingLabel = ""; // Armazena rótulos que ficaram isolados em uma linha

    while (std::getline(file, currentLine)) {
        lineNumber++;
        
        // 1. Limpar comentários e padronizar para MAIÚSCULAS
        std::string clean = stripComment(currentLine);
        clean = toUpper(clean);

        // 2. Tokenizar a linha usando espaços/tabs
        std::stringstream ss(clean);
        std::vector<std::string> tokens;
        std::string token;
        while (ss >> token) {
            tokens.push_back(token);
        }

        // Se a linha for vazia ou contiver apenas comentários, ignora
        if (tokens.empty()) {
            continue;
        }

        LineData lineData;
        lineData.originalLineNumber = lineNumber;

        size_t tokenIndex = 0;

        // 3. Verificar se o primeiro token é um Rótulo (termina com ':')
        if (tokens[tokenIndex].back() == ':') {
            std::string labelName = tokens[tokenIndex].substr(0, tokens[tokenIndex].size() - 1);
            
            // Validar Léxico do Rótulo
            std::string err;
            if (!isValidLabel(labelName, err)) {
                errors.push_back({lineNumber, "LEXICO", err});
            }

            if (!pendingLabel.empty()) {
                errors.push_back({lineNumber, "SEMANTICO", "Dois rótulos consecutivos na mesma instrução"});
            }

            pendingLabel = labelName;
            tokenIndex++;
        }

        // Se a linha continha APENAS o rótulo (ex: "ROT:"), guarda o rótulo e vai pra próxima linha
        if (tokenIndex >= tokens.size()) {
            continue;
        }

        // Associa o rótulo (pendente ou da linha atual) à instrução atual
        if (!pendingLabel.empty()) {
            lineData.label = pendingLabel;
            pendingLabel = ""; // Reseta a pendência
        }

        // 4. Capturar a Instrução/Diretiva
        lineData.instruction = tokens[tokenIndex++];

        // 5. Capturar Argumentos/Operandos
        while (tokenIndex < tokens.size()) {
            lineData.args.push_back(tokens[tokenIndex++]);
        }

        parsedLines.push_back(lineData);
    }

    // Erro caso o arquivo termine com um rótulo pendente sem instrução
    if (!pendingLabel.empty()) {
        errors.push_back({lineNumber, "SINTATICO", "Rótulo '" + pendingLabel + "' no final do arquivo sem instrução associada"});
    }

    file.close();
}