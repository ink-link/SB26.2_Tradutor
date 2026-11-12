#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct InstructionInfo {
    int opcode;
    int size;
    int operands;
};

struct AssemblyError {
    int lineNumber;
    std::string type;
    std::string message;
};

struct SourceLine {
    int lineNumber;
    std::string original;
    std::vector<std::string> tokens;
};

struct LineData {
    int originalLineNumber = 0;
    std::vector<std::string> labels;
    std::vector<int> labelLines;
    std::string instruction;
    std::vector<std::string> args;
};

enum class Section { None, Text, Data };

struct Statement {
    int line = 0;
    std::vector<std::string> labels;
    std::vector<int> labelLines;
    std::string op;
    std::vector<std::string> args;
    Section section = Section::None;
    bool valid = true;
};

struct ObjectModule {
    std::string name;
    bool isModule = false;
    std::vector<int> code;
    std::vector<bool> reloc;
    std::vector<std::pair<std::string, int>> defs;
    std::vector<std::pair<std::string, int>> uses;
    std::vector<std::string> externs;
};

inline const std::string OBJ_EXT = ".obj";
inline const std::string LIG_EXT = ".lig";
inline constexpr int MEMORY_WORDS = 1 << 16;

inline const std::unordered_map<std::string, InstructionInfo> OPCODE_TABLE = {
    {"ADD", {1, 2, 1}}, {"SUB", {2, 2, 1}}, {"MUL", {3, 2, 1}},
    {"MULT", {3, 2, 1}}, {"DIV", {4, 2, 1}}, {"JMP", {5, 2, 1}},
    {"JMPN", {6, 2, 1}}, {"JMPP", {7, 2, 1}}, {"JMPZ", {8, 2, 1}},
    {"COPY", {9, 3, 2}}, {"LOAD", {10, 2, 1}}, {"STORE", {11, 2, 1}},
    {"INPUT", {12, 2, 1}}, {"OUTPUT", {13, 2, 1}}, {"STOP", {14, 1, 0}}
};

#endif
