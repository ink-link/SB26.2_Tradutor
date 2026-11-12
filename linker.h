#ifndef LINKER_H
#define LINKER_H

#include "structures.h"

#include <string>
#include <vector>

bool linkModules(const std::vector<std::string>& paths,
                 std::vector<int>& executable,
                 std::vector<AssemblyError>& assemblyErrors,
                 std::vector<std::string>& linkErrors);

#endif
