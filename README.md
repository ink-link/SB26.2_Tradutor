# Montador e ligador

Projeto 1 de Software Básico, Universidade de Brasília.

## Compilação

É necessário um compilador com suporte a C++17:

```sh
make
```

O comando gera os executáveis `montador` e `ligador`. Para executar os testes:

```sh
make test
```

## Uso

Monte um programa ou módulo:

```sh
./montador programa.asm
```

O arquivo de saída será `programa.obj`. Para ligar de um a três módulos, na
ordem desejada:

```sh
./ligador a.asm [b.asm [c.asm]]
```

O ligador gera `a.lig`. Se houver erros, eles são apresentados na saída padrão
e o arquivo de saída não é gerado.
