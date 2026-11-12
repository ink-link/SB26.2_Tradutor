#ifndef LEXER_H
#define LEXER_H

#include "structures.h"

#include <string>
#include <vector>

std::string toUpper(std::string value);
bool isValidLabel(const std::string& label, std::string& errorMsg);
void readSourceLines(const std::string& filename,
                     std::vector<SourceLine>& lines,
                     std::vector<AssemblyError>& errors);
void parseFile(const std::string& filename,
               std::vector<LineData>& lines,
               std::vector<AssemblyError>& errors);

#endif
