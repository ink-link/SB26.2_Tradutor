#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

// Converte a string para caixa alta (case-insensitive)
std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

// Remove comentários iniciados por ';'
std::string stripComments(const std::string& line) {
    size_t pos = line.find(';');
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

// Divide a linha em tokens usando espaço/tabulação como delimitador
std::vector<std::string> tokenize(const std::string& line) {
    std::string cleanLine = stripComments(line);
    cleanLine = toUpper(cleanLine);
    
    std::vector<std::string> tokens;
    std::stringstream ss(cleanLine);
    std::string token;
    
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}