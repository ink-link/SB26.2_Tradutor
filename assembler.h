#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "structures.h"

#include <string>
#include <vector>

ObjectModule assemble(const std::string& path, std::vector<AssemblyError>& errors);

#endif
