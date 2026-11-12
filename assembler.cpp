#include "assembler.h"

#include "lexer.h"

#include <algorithm>
#include <climits>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr bool COPY_COMMA_IS_ERROR = true;
constexpr bool MODULE_MARKERS_ENABLE_MODULES = true;

struct Symbol {
    int value = 0;
    bool external = false;
    Section section = Section::None;
    int line = 0;
};

struct Operand {
    std::string name;
    int addend = 0;
};

bool parseNumber(const std::string& token, int& value) {
    if (token.empty()) {
        return false;
    }
    std::size_t index = (token[0] == '+' || token[0] == '-') ? 1 : 0;
    if (index == token.size()) {
        return false;
    }
    for (std::size_t i = index; i < token.size(); ++i) {
        if (token[i] < '0' || token[i] > '9') {
            return false;
        }
    }
    try {
        const long long parsed = std::stoll(token);
        if (parsed < INT_MIN || parsed > INT_MAX) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

void addError(std::vector<AssemblyError>& errors, int line,
              const std::string& type, const std::string& message) {
    errors.push_back({line, type, message});
}

bool labelToken(const std::string& token, std::string& label) {
    if (token.empty() || token.back() != ':') {
        return false;
    }
    label = token.substr(0, token.size() - 1);
    return true;
}

bool isModuleMarker(const std::string& op) {
    return MODULE_MARKERS_ENABLE_MODULES &&
           (op == "BEGIN" || op == "PUBLIC" || op == "EXTERN");
}

bool parseOperand(const std::vector<std::string>& tokens, Operand& operand,
                  int line, std::vector<AssemblyError>& errors) {
    if (tokens.empty() || tokens.size() > 3) {
        addError(errors, line, "SINTATICO", "operando malformado");
        return false;
    }
    if (std::any_of(tokens.begin(), tokens.end(), [](const std::string& token) {
            return token.find(',') != std::string::npos;
        })) {
        addError(errors, line, "SINTATICO", "virgula nao permitida no operando");
        return false;
    }

    std::string label;
    std::string number;
    if (tokens.size() == 1) {
        const std::size_t plus = tokens[0].find('+');
        if (plus == std::string::npos) {
            label = tokens[0];
        } else {
            label = tokens[0].substr(0, plus);
            number = tokens[0].substr(plus + 1);
        }
    } else if (tokens.size() == 2) {
        if (tokens[1] == "+") {
            addError(errors, line, "SINTATICO", "'+' sem numero");
            return false;
        }
        if (tokens[0].back() == '+') {
            label = tokens[0].substr(0, tokens[0].size() - 1);
            number = tokens[1];
        } else {
            addError(errors, line, "SINTATICO", "operando malformado");
            return false;
        }
    } else {
        if (tokens[1] != "+") {
            addError(errors, line, "SINTATICO", "operando malformado");
            return false;
        }
        label = tokens[0];
        number = tokens[2];
    }

    std::string message;
    if (!isValidLabel(label, message)) {
        addError(errors, line, "LEXICO", message);
        return false;
    }
    if (!number.empty()) {
        if (!parseNumber(number, operand.addend)) {
            addError(errors, line, "LEXICO", "numero invalido '" + number + "'");
            return false;
        }
        if (operand.addend < 0) {
            addError(errors, line, "SINTATICO", "o deslocamento deve ser nao negativo");
            return false;
        }
    } else if ((!tokens.empty() && (tokens.back() == "+" ||
                                    (!tokens[0].empty() && tokens[0].back() == '+')))) {
        addError(errors, line, "SINTATICO", "'+' sem numero");
        return false;
    }
    operand.name = label;
    return true;
}

bool resolvePreprocessorValue(const std::string& token,
                              const std::unordered_map<std::string, int>& equates,
                              int& value) {
    if (parseNumber(token, value)) {
        return true;
    }
    const auto found = equates.find(token);
    if (found == equates.end()) {
        return false;
    }
    value = found->second;
    return true;
}

std::vector<SourceLine> preprocess(
    const std::vector<SourceLine>& source,
    std::unordered_map<std::string, int>& equates,
    std::vector<AssemblyError>& errors) {
    std::vector<SourceLine> active;
    Section section = Section::None;

    for (std::size_t i = 0; i < source.size(); ++i) {
        const SourceLine& line = source[i];
        std::string label;
        const bool hasLabel = labelToken(line.tokens[0], label);
        const std::size_t operation = hasLabel ? 1 : 0;
        if (operation >= line.tokens.size()) {
            active.push_back(line);
            continue;
        }
        const std::string& op = line.tokens[operation];

        if (hasLabel && op == "EQU") {
            std::string message;
            if (!isValidLabel(label, message)) {
                addError(errors, line.lineNumber, "LEXICO", message);
            }
            if (section != Section::None) {
                addError(errors, line.lineNumber, "SEMANTICO",
                         "EQU deve ser declarado antes das secoes");
            }
            if (line.tokens.size() != operation + 2) {
                addError(errors, line.lineNumber, "SINTATICO",
                         "EQU exige um valor");
            } else {
                int value = 0;
                if (!parseNumber(line.tokens[operation + 1], value)) {
                    addError(errors, line.lineNumber, "LEXICO",
                             "numero invalido em EQU");
                } else if (equates.find(label) != equates.end()) {
                    addError(errors, line.lineNumber, "SEMANTICO",
                             "EQU '" + label + "' declarado mais de uma vez");
                } else {
                    equates.emplace(label, value);
                }
            }
            continue;
        }
        if (!hasLabel && op == "IF") {
            if (section != Section::Text) {
                addError(errors, line.lineNumber, "SEMANTICO",
                         "IF permitido somente na secao TEXT");
            }
            int value = 0;
            bool valid = true;
            if (line.tokens.size() != operation + 2) {
                addError(errors, line.lineNumber, "SINTATICO",
                         "IF exige um operando");
                valid = false;
            } else if (!resolvePreprocessorValue(line.tokens[operation + 1],
                                                 equates, value)) {
                const std::string& token = line.tokens[operation + 1];
                const bool numeric = !token.empty() &&
                    (std::isdigit(static_cast<unsigned char>(token[0])) ||
                     token[0] == '+' || token[0] == '-');
                addError(errors, line.lineNumber,
                         numeric ? "LEXICO" : "SEMANTICO",
                         numeric ? "numero invalido em IF"
                                 : "valor de IF nao declarado");
                valid = false;
            }
            if (valid && value == 0 && i + 1 < source.size()) {
                ++i;
            }
            continue;
        }

        active.push_back(line);
        if (!hasLabel && (op == "SECAO" || op == "SECTION") &&
            line.tokens.size() == operation + 2) {
            if (line.tokens[operation + 1] == "TEXT") {
                section = Section::Text;
            } else if (line.tokens[operation + 1] == "DATA") {
                section = Section::Data;
            }
        }
    }
    return active;
}

bool parseConstant(const std::string& token,
                   const std::unordered_map<std::string, int>& equates,
                   int& value, int line, std::vector<AssemblyError>& errors) {
    if (parseNumber(token, value)) {
        return true;
    }
    if (equates.find(token) != equates.end()) {
        value = equates.at(token);
        return true;
    }
    bool looksNumeric = !token.empty() &&
        (std::isdigit(static_cast<unsigned char>(token[0])) ||
         token[0] == '+' || token[0] == '-');
    if (looksNumeric) {
        addError(errors, line, "LEXICO", "numero invalido '" + token + "'");
    } else {
        addError(errors, line, "SEMANTICO", "constante '" + token + "' nao declarada");
    }
    return false;
}

} // namespace

ObjectModule assemble(const std::string& path, std::vector<AssemblyError>& errors) {
    ObjectModule object;
    object.name = std::filesystem::path(path).stem().string();
    std::vector<SourceLine> source;
    readSourceLines(path, source, errors);
    if (!errors.empty()) {
        return object;
    }

    std::unordered_map<std::string, int> equates;
    const std::vector<SourceLine> active = preprocess(source, equates, errors);
    std::vector<Statement> statements;
    std::vector<std::string> pendingLabels;
    std::vector<int> pendingLines;
    Section currentSection = Section::None;
    bool hasText = false;

    for (const SourceLine& line : active) {
        std::size_t index = 0;
        std::vector<std::string> labels;
        std::vector<int> labelLines;
        while (index < line.tokens.size()) {
            std::string label;
            if (!labelToken(line.tokens[index], label)) {
                break;
            }
            std::string message;
            if (!isValidLabel(label, message)) {
                addError(errors, line.lineNumber, "LEXICO", message);
            }
            labels.push_back(label);
            labelLines.push_back(line.lineNumber);
            ++index;
        }
        if (labels.size() > 1) {
            addError(errors, line.lineNumber, "SINTATICO",
                     "dois rotulos na mesma linha");
        }
        if (index == line.tokens.size()) {
            pendingLabels.insert(pendingLabels.end(), labels.begin(), labels.end());
            pendingLines.insert(pendingLines.end(), labelLines.begin(),
                                labelLines.end());
            continue;
        }
        labels.insert(labels.begin(), pendingLabels.begin(), pendingLabels.end());
        labelLines.insert(labelLines.begin(), pendingLines.begin(),
                          pendingLines.end());
        pendingLabels.clear();
        pendingLines.clear();

        Statement statement;
        statement.line = line.lineNumber;
        statement.labels = std::move(labels);
        statement.labelLines = std::move(labelLines);
        statement.op = line.tokens[index++];
        statement.args.assign(line.tokens.begin() + static_cast<std::ptrdiff_t>(index),
                              line.tokens.end());
        statement.section = currentSection;

        if (statement.op == "SECAO" || statement.op == "SECTION") {
            if (!statement.labels.empty()) {
                addError(errors, statement.line, "SEMANTICO",
                         "rotulo nao permitido na declaracao de secao");
                statement.valid = false;
            }
            if (statement.args.size() != 1 ||
                (statement.args[0] != "TEXT" && statement.args[0] != "DATA")) {
                addError(errors, statement.line, "SINTATICO",
                         "SECAO exige TEXT ou DATA");
                statement.valid = false;
            } else {
                currentSection = statement.args[0] == "TEXT"
                                     ? Section::Text : Section::Data;
                statement.section = currentSection;
                hasText = hasText || currentSection == Section::Text;
            }
        } else if (statement.op == "BEGIN") {
            object.isModule = object.isModule || isModuleMarker(statement.op);
            if (statement.args.size() != 0 || statement.labels.size() != 1) {
                addError(errors, statement.line, "SINTATICO",
                         "BEGIN exige um rotulo e nao aceita operandos");
                statement.valid = false;
            } else {
                object.name = statement.labels.front();
            }
        } else if (statement.op == "END") {
            if (!statement.labels.empty() || !statement.args.empty()) {
                addError(errors, statement.line, "SINTATICO",
                         "END nao aceita rotulo ou operandos");
                statement.valid = false;
            }
        } else if (statement.op == "EXTERN") {
            object.isModule = object.isModule || isModuleMarker(statement.op);
            if (statement.args.size() != 0 || statement.labels.size() != 1) {
                addError(errors, statement.line, "SINTATICO",
                         "EXTERN exige um rotulo e nao aceita operandos");
                statement.valid = false;
            }
        } else if (statement.op == "PUBLIC") {
            object.isModule = object.isModule || isModuleMarker(statement.op);
            if (!statement.labels.empty() || statement.args.size() != 1) {
                addError(errors, statement.line, "SINTATICO",
                         "PUBLIC exige um simbolo");
                statement.valid = false;
            } else {
                std::string message;
                if (!isValidLabel(statement.args[0], message)) {
                    addError(errors, statement.line, "LEXICO", message);
                    statement.valid = false;
                }
            }
        } else if (statement.op == "CONST" || statement.op == "SPACE") {
            if (currentSection != Section::Data) {
                addError(errors, statement.line, "SEMANTICO",
                         statement.op + " permitido somente na secao DATA");
                statement.valid = false;
            }
            if (statement.args.size() > 1 ||
                (statement.op == "CONST" && statement.args.empty())) {
                addError(errors, statement.line, "SINTATICO",
                         statement.op + " com quantidade incorreta de operandos");
                statement.valid = false;
            } else if (!statement.args.empty()) {
                int value = 0;
                if (statement.op == "CONST") {
                    statement.valid = parseConstant(statement.args[0], equates,
                                                    value, statement.line, errors) &&
                                      statement.valid;
                } else if (!parseConstant(statement.args[0], equates, value,
                                          statement.line, errors)) {
                    statement.valid = false;
                } else if (value < 1) {
                    addError(errors, statement.line, "SEMANTICO",
                             "SPACE exige quantidade maior que zero");
                    statement.valid = false;
                }
            }
        } else if (OPCODE_TABLE.find(statement.op) != OPCODE_TABLE.end()) {
            const auto& info = OPCODE_TABLE.at(statement.op);
            if (currentSection != Section::Text) {
                addError(errors, statement.line, "SEMANTICO",
                         "instrucao permitida somente na secao TEXT");
                statement.valid = false;
            }
            if (statement.op == "COPY") {
                if (statement.args.size() != 2) {
                    addError(errors, statement.line, "SINTATICO",
                             "COPY exige dois operandos separados por espaco");
                    statement.valid = false;
                }
                for (const std::string& arg : statement.args) {
                    if (COPY_COMMA_IS_ERROR &&
                        arg.find(',') != std::string::npos) {
                        addError(errors, statement.line, "SINTATICO",
                                 "virgula nao permitida em COPY");
                        statement.valid = false;
                    }
                }
            } else if (info.operands == 0 && !statement.args.empty()) {
                addError(errors, statement.line, "SINTATICO",
                         statement.op + " com quantidade incorreta de operandos");
                statement.valid = false;
            }
            if (statement.op != "COPY" && info.operands == 1 &&
                !statement.args.empty()) {
                Operand parsed;
                std::vector<AssemblyError> operandErrors;
                if (!parseOperand(statement.args, parsed, statement.line,
                                  operandErrors)) {
                    statement.valid = false;
                }
                errors.insert(errors.end(), operandErrors.begin(),
                              operandErrors.end());
            } else if (statement.op != "COPY" && info.operands == 1) {
                addError(errors, statement.line, "SINTATICO",
                         statement.op + " exige um operando");
                statement.valid = false;
            } else if (statement.op == "COPY" &&
                       statement.args.size() == 2) {
                for (const std::string& arg : statement.args) {
                    Operand parsed;
                    std::vector<AssemblyError> operandErrors;
                    if (!parseOperand({arg}, parsed, statement.line,
                                      operandErrors)) {
                        statement.valid = false;
                    }
                    errors.insert(errors.end(), operandErrors.begin(),
                                  operandErrors.end());
                }
            }
        } else {
            addError(errors, statement.line, "SINTATICO",
                     "operacao ou diretiva inexistente '" + statement.op + "'");
            statement.valid = false;
        }

        if (statement.op == "IF" || statement.op == "EQU") {
            addError(errors, statement.line, "SINTATICO",
                     "diretiva malformada");
            statement.valid = false;
        }
        statements.push_back(std::move(statement));
    }

    if (!pendingLabels.empty()) {
        addError(errors, pendingLines.back(), "SINTATICO",
                 "rotulo no fim do arquivo sem operacao associada");
    }
    if (!hasText) {
        addError(errors, source.empty() ? 1 : source.back().lineNumber,
                 "SEMANTICO", "secao TEXT ausente");
    }

    std::unordered_map<std::string, Symbol> symbols;
    std::unordered_set<std::string> publicNames;
    int textSize = 0;
    int dataSize = 0;
    for (Statement& statement : statements) {
        if (!statement.valid) {
            continue;
        }
        if (statement.op == "BEGIN") {
            if (statement.labels.size() == 1) {
                const std::string& name = statement.labels.front();
                if (symbols.find(name) != symbols.end()) {
                    addError(errors, statement.line, "SEMANTICO",
                             "rotulo '" + name + "' declarado mais de uma vez");
                } else {
                    symbols.emplace(name, Symbol{0, false, Section::Text,
                                                  statement.labelLines.front()});
                }
            }
            continue;
        }
        if (statement.op == "END" || statement.op == "PUBLIC" ||
            statement.op == "SECAO" || statement.op == "SECTION") {
            continue;
        }
        if (statement.op == "EXTERN") {
            const std::string& name = statement.labels.front();
            object.externs.push_back(name);
            if (symbols.find(name) != symbols.end()) {
                addError(errors, statement.line, "SEMANTICO",
                         "simbolo '" + name + "' declarado mais de uma vez");
            } else {
                symbols.emplace(name, Symbol{0, true, Section::None, statement.line});
            }
            continue;
        }
        for (std::size_t i = 0; i < statement.labels.size(); ++i) {
            const std::string& name = statement.labels[i];
            const int declarationLine = statement.labelLines[i];
            if (symbols.find(name) != symbols.end()) {
                addError(errors, declarationLine, "SEMANTICO",
                         "rotulo '" + name + "' declarado mais de uma vez");
            } else if (statement.section == Section::Text) {
                symbols.emplace(name, Symbol{textSize, false, Section::Text,
                                              declarationLine});
            } else if (statement.section == Section::Data) {
                symbols.emplace(name, Symbol{dataSize, false, Section::Data,
                                              declarationLine});
            } else {
                addError(errors, declarationLine, "SEMANTICO",
                         "rotulo fora de uma secao");
            }
        }
        if (statement.section == Section::Text &&
            OPCODE_TABLE.find(statement.op) != OPCODE_TABLE.end()) {
            const int size = OPCODE_TABLE.at(statement.op).size;
            if (size > MEMORY_WORDS - textSize - dataSize) {
                addError(errors, statement.line, "SEMANTICO",
                         "programa excede o limite de memoria");
                statement.valid = false;
            } else {
                textSize += size;
            }
        } else if (statement.section == Section::Data &&
                   (statement.op == "CONST" || statement.op == "SPACE")) {
            int count = 1;
            if (statement.op == "SPACE" && !statement.args.empty()) {
                parseConstant(statement.args[0], equates, count, statement.line,
                              errors);
            }
            if (count > MEMORY_WORDS - textSize - dataSize) {
                addError(errors, statement.line, "SEMANTICO",
                         "programa excede o limite de memoria");
                statement.valid = false;
            } else {
                dataSize += count;
            }
        }
    }

    for (const Statement& statement : statements) {
        if (!statement.valid || statement.op != "PUBLIC") {
            continue;
        }
        const std::string& name = statement.args.front();
        const auto symbol = symbols.find(name);
        if (symbol == symbols.end() || symbol->second.external) {
            addError(errors, statement.line, "SEMANTICO",
                     "PUBLIC '" + name + "' nao definido localmente");
        } else if (publicNames.insert(name).second) {
            const int value = symbol->second.section == Section::Data
                                  ? textSize + symbol->second.value
                                  : symbol->second.value;
            object.defs.emplace_back(name, value);
        }
    }
    for (const auto& entry : symbols) {
        if (!entry.second.external && entry.second.section == Section::Data) {
            symbols[entry.first].value += textSize;
        }
    }

    std::vector<int> textCode;
    std::vector<bool> textReloc;
    std::vector<int> dataCode;
    for (const Statement& statement : statements) {
        if (!statement.valid) {
            continue;
        }
        if (OPCODE_TABLE.find(statement.op) != OPCODE_TABLE.end() &&
            statement.section == Section::Text) {
            const InstructionInfo info = OPCODE_TABLE.at(statement.op);
            textCode.push_back(info.opcode);
            textReloc.push_back(false);
            if (statement.op == "COPY") {
                for (const std::string& token : statement.args) {
                    Operand operand;
                    if (!parseOperand({token}, operand, statement.line, errors)) {
                        continue;
                    }
                    const auto found = symbols.find(operand.name);
                    if (found == symbols.end()) {
                        addError(errors, statement.line, "SEMANTICO",
                                 "rotulo '" + operand.name + "' nao declarado");
                        textCode.push_back(operand.addend);
                        textReloc.push_back(false);
                    } else {
                        const int position = static_cast<int>(textCode.size());
                        if (found->second.external) {
                            textCode.push_back(operand.addend);
                            textReloc.push_back(false);
                            object.uses.emplace_back(operand.name, position);
                        } else {
                            textCode.push_back(found->second.value + operand.addend);
                            textReloc.push_back(true);
                        }
                    }
                }
            } else if (info.operands == 1) {
                Operand operand;
                if (!parseOperand(statement.args, operand, statement.line, errors)) {
                    continue;
                }
                const auto found = symbols.find(operand.name);
                if (found == symbols.end()) {
                    addError(errors, statement.line, "SEMANTICO",
                             "rotulo '" + operand.name + "' nao declarado");
                    textCode.push_back(operand.addend);
                    textReloc.push_back(false);
                } else {
                    const int position = static_cast<int>(textCode.size());
                    if (found->second.external) {
                        textCode.push_back(operand.addend);
                        textReloc.push_back(false);
                        object.uses.emplace_back(operand.name, position);
                    } else {
                        textCode.push_back(found->second.value + operand.addend);
                        textReloc.push_back(true);
                    }
                }
            }
        } else if (statement.op == "CONST" &&
                   statement.section == Section::Data) {
            int value = 0;
            if (parseConstant(statement.args[0], equates, value,
                              statement.line, errors)) {
                dataCode.push_back(value);
            }
        } else if (statement.op == "SPACE" &&
                   statement.section == Section::Data) {
            int count = 1;
            if (!statement.args.empty()) {
                parseConstant(statement.args[0], equates, count,
                              statement.line, errors);
            }
            dataCode.insert(dataCode.end(), static_cast<std::size_t>(count), 0);
        }
    }

    object.code = std::move(textCode);
    object.reloc = std::move(textReloc);
    object.code.insert(object.code.end(), dataCode.begin(), dataCode.end());
    object.reloc.insert(object.reloc.end(), dataCode.size(), false);
    return object;
}
