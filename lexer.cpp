#include "lexer.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
std::string stripComment(const std::string& line) {
    const std::size_t position = line.find(';');
    return position == std::string::npos ? line : line.substr(0, position);
}
}

std::string toUpper(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character)));
    }
    return value;
}

bool isValidLabel(const std::string& label, std::string& errorMsg) {
    if (label.empty()) {
        errorMsg = "rotulo vazio";
        return false;
    }
    if (label.size() > 30) {
        errorMsg = "rotulo '" + label + "' excede 30 caracteres";
        return false;
    }
    const auto first = static_cast<unsigned char>(label.front());
    if (!std::isalpha(first) && label.front() != '_') {
        errorMsg = "rotulo '" + label + "' deve comecar com letra ou '_'";
        return false;
    }
    for (char character : label) {
        const auto value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '_') {
            errorMsg = "caractere invalido no rotulo '" + label + "'";
            return false;
        }
    }
    return true;
}

void readSourceLines(const std::string& filename,
                     std::vector<SourceLine>& lines,
                     std::vector<AssemblyError>& errors) {
    std::ifstream file(filename);
    if (!file) {
        errors.push_back({1, "SINTATICO", "nao foi possivel abrir o arquivo '" +
                                              filename + "'"});
        return;
    }

    std::string original;
    int number = 0;
    while (std::getline(file, original)) {
        ++number;
        const std::string clean = stripComment(original);
        std::istringstream stream(clean);
        SourceLine line{number, original, {}};
        std::string token;
        while (stream >> token) {
            line.tokens.push_back(toUpper(token));
        }
        if (!line.tokens.empty()) {
            lines.push_back(std::move(line));
        }
    }
}

void parseFile(const std::string& filename,
               std::vector<LineData>& lines,
               std::vector<AssemblyError>& errors) {
    std::vector<SourceLine> source;
    readSourceLines(filename, source, errors);
    std::vector<std::string> pendingLabels;
    std::vector<int> pendingLines;

    for (const SourceLine& sourceLine : source) {
        std::size_t index = 0;
        LineData line;
        line.originalLineNumber = sourceLine.lineNumber;
        while (index < sourceLine.tokens.size() &&
               sourceLine.tokens[index].back() == ':') {
            std::string label = sourceLine.tokens[index].substr(
                0, sourceLine.tokens[index].size() - 1);
            std::string message;
            if (!isValidLabel(label, message)) {
                errors.push_back({sourceLine.lineNumber, "LEXICO", message});
            }
            if (index > 0) {
                errors.push_back({sourceLine.lineNumber, "SINTATICO",
                                  "dois rotulos na mesma linha"});
            }
            line.labels.push_back(label);
            line.labelLines.push_back(sourceLine.lineNumber);
            ++index;
        }
        if (index == sourceLine.tokens.size()) {
            pendingLabels.insert(pendingLabels.end(), line.labels.begin(),
                                 line.labels.end());
            pendingLines.insert(pendingLines.end(), line.labelLines.begin(),
                                line.labelLines.end());
            continue;
        }
        if (index > 0 && index + 1 < sourceLine.tokens.size() &&
            sourceLine.tokens[index + 1].back() == ':') {
            errors.push_back({sourceLine.lineNumber, "SINTATICO",
                              "dois rotulos na mesma linha"});
        }
        line.labels.insert(line.labels.begin(), pendingLabels.begin(),
                           pendingLabels.end());
        line.labelLines.insert(line.labelLines.begin(), pendingLines.begin(),
                               pendingLines.end());
        pendingLabels.clear();
        pendingLines.clear();
        line.instruction = sourceLine.tokens[index++];
        line.args.assign(sourceLine.tokens.begin() + static_cast<std::ptrdiff_t>(index),
                         sourceLine.tokens.end());
        lines.push_back(std::move(line));
    }
    if (!pendingLabels.empty()) {
        errors.push_back({pendingLines.back(), "SINTATICO",
                          "rotulo no fim do arquivo sem operacao"});
    }
}
