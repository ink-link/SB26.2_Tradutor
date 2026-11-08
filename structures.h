#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <string>
#include <vector>
#include <unordered_map>

// Informações de cada instrução hipotética
struct InstructionInfo {
    int opcode;
    int size;       // Tamanho em palavras (ex: 2 para ADD X)
    int operands;   // Quantidade de operandos esperados
};

// Registro de erro identificado
struct AssemblyError {
    int lineNumber;
    std::string type;    // "LEXICO", "SINTATICO" ou "SEMANTICO"
    std::string message;
};

// Tabela de Opcodes estática (14 instruções da máquina hipotética)
const std::unordered_map<std::string, InstructionInfo> OPCODE_TABLE = {
    {"ADD",    {1,  2, 1}},
    {"SUB",    {2,  2, 1}},
    {"MULT",   {3,  2, 1}},
    {"DIV",    {4,  2, 1}},
    {"JMP",    {5,  2, 1}},
    {"JMPN",   {6,  2, 1}},
    {"JMPP",   {7,  2, 1}},
    {"ZE RO",  {8,  2, 1}}, // Ajustar para o mnemônico exato das aulas (ex: JMPZ)
    {"COPY",   {9,  3, 2}},
    {"LOAD",   {10, 2, 1}},
    {"STORE",  {11, 2, 1}},
    {"INPUT",  {12, 2, 1}},
    {"OUTPUT", {13, 2, 1}},
    {"STOP",   {14, 1, 0}}
};

#endif