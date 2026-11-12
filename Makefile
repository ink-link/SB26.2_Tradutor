CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra

COMMON = assembler.cpp lexer.cpp

.PHONY: all test clean

all: montador ligador

montador: main.cpp $(COMMON) assembler.h lexer.h structures.h
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(COMMON)

ligador: main_ligador.cpp linker.cpp $(COMMON) linker.h assembler.h lexer.h structures.h
	$(CXX) $(CXXFLAGS) -o $@ main_ligador.cpp linker.cpp $(COMMON)

test:
	bash tests/run_tests.sh

clean:
	$(RM) montador montador.exe ligador ligador.exe
