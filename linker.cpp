#include "linker.h"

#include "assembler.h"

#include <unordered_set>
#include <unordered_map>

bool linkModules(const std::vector<std::string>& paths,
                 std::vector<int>& executable,
                 std::vector<AssemblyError>& assemblyErrors,
                 std::vector<std::string>& linkErrors) {
    std::vector<ObjectModule> modules;
    modules.reserve(paths.size());
    for (const std::string& path : paths) {
        std::vector<AssemblyError> errors;
        modules.push_back(assemble(path, errors));
        assemblyErrors.insert(assemblyErrors.end(), errors.begin(), errors.end());
    }
    if (!assemblyErrors.empty()) {
        return false;
    }

    std::vector<int> factors;
    std::unordered_map<std::string, int> definitions;
    int totalSize = 0;
    for (const ObjectModule& module : modules) {
        factors.push_back(totalSize);
        for (const auto& definition : module.defs) {
            const auto inserted = definitions.emplace(
                definition.first, definition.second + totalSize);
            if (!inserted.second) {
                linkErrors.push_back("simbolo PUBLIC '" + definition.first +
                                     "' definido em mais de um modulo");
            }
        }
        totalSize += static_cast<int>(module.code.size());
    }
    if (totalSize > MEMORY_WORDS) {
        linkErrors.push_back("programa ligado excede o limite de memoria");
    }
    if (!linkErrors.empty()) {
        return false;
    }

    std::unordered_set<std::string> missingSymbols;
    for (const ObjectModule& module : modules) {
        for (const std::string& external : module.externs) {
            if (definitions.find(external) == definitions.end() &&
                missingSymbols.insert(external).second) {
                linkErrors.push_back("simbolo EXTERN '" + external +
                                     "' nao definido");
            }
        }
        for (const auto& use : module.uses) {
            if (definitions.find(use.first) == definitions.end() &&
                missingSymbols.insert(use.first).second) {
                linkErrors.push_back("simbolo EXTERN '" + use.first +
                                     "' nao definido");
            }
        }
    }
    if (!linkErrors.empty()) {
        return false;
    }

    executable.clear();
    executable.reserve(static_cast<std::size_t>(totalSize));
    for (std::size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const ObjectModule& module = modules[moduleIndex];
        std::vector<int> code = module.code;
        for (const auto& use : module.uses) {
            const int position = use.second;
            if (position < 0 || static_cast<std::size_t>(position) >= code.size()) {
                linkErrors.push_back("posicao de uso invalida para '" + use.first + "'");
                continue;
            }
            code[static_cast<std::size_t>(position)] += definitions.at(use.first);
            const int address = code[static_cast<std::size_t>(position)];
            if (address < 0 || address >= totalSize) {
                linkErrors.push_back("endereco resultante fora dos limites: " +
                                     std::to_string(address));
            }
        }
        for (std::size_t i = 0; i < module.reloc.size() && i < code.size(); ++i) {
            if (module.reloc[i]) {
                code[i] += factors[moduleIndex];
                if (code[i] < 0 || code[i] >= totalSize) {
                    linkErrors.push_back("endereco resultante fora dos limites: " +
                                         std::to_string(code[i]));
                }
            }
        }
        executable.insert(executable.end(), code.begin(), code.end());
    }
    return linkErrors.empty();
}
