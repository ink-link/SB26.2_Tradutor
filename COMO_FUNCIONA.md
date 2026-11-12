# Como funcionam o montador e o ligador

Este documento descreve o caminho percorrido por um arquivo `.asm`, desde a
leitura do texto ate a geracao do objeto ou do executavel ligado. Os programas
geram codigo para a maquina hipotetica da atividade; eles nao executam esse
codigo.

## 1. Compilar e executar

O projeto usa C++17 e somente a biblioteca padrao. Na raiz do projeto:

```sh
make
```

Isso compila os executaveis `montador` e `ligador`. No Windows, o compilador
normalmente acrescenta a extensao `.exe`.

Para montar um arquivo:

```sh
./montador programa.asm
```

O montador cria `programa.obj` ao lado do arquivo-fonte. Para ligar de um a
tres fontes, na ordem desejada:

```sh
./ligador modulo_a.asm modulo_b.asm
```

O ligador monta internamente cada fonte e cria `modulo_a.lig`, usando o nome
do primeiro argumento. A extensao `.asm` deve ser informada no comando.

Os testes podem ser executados com:

```sh
make test
```

Se houver erros de montagem ou ligacao, eles sao enviados para a saida padrao,
o programa termina com codigo diferente de zero e nao grava um novo arquivo de
saida.

## 2. Componentes do projeto

| Arquivo | Responsabilidade |
|---|---|
| `structures.h` | Tipos compartilhados, tabela de opcodes, secoes e extensoes de saida. |
| `lexer.h` / `lexer.cpp` | Leitura das linhas, remocao de comentarios, tokenizacao e validacao de rotulos. |
| `assembler.h` / `assembler.cpp` | Pre-processamento, analise, tabela de simbolos e geracao do objeto. |
| `linker.h` / `linker.cpp` | Montagem interna dos modulos, resolucao de simbolos e relocacao. |
| `main.cpp` | Argumentos, formatacao e gravacao do `montador`. |
| `main_ligador.cpp` | Argumentos, formatacao e gravacao do `ligador`. |
| `Makefile` | Alvos `all`, `montador`, `ligador`, `test` e `clean`. |
| `tests/run_tests.sh` | Testes de montagem simples, modulos, ligacao, erros e pre-processamento. |

O contrato compartilhado pelos dois executaveis e `assemble(path, errors)`,
declarado em `assembler.h`. O ligador chama essa funcao para cada fonte; nao
inicia um segundo processo nem precisa ler os `.obj` intermediarios do disco.

## 3. Fluxo do montador

O montador segue estas etapas:

1. **Leitura e tokenizacao.** Cada linha recebe seu numero original. O texto
   depois de `;` e removido, linhas vazias sao ignoradas e os tokens sao
   normalizados para maiusculas. Espacos e tabulacoes funcionam como
   separadores.
2. **Pre-processamento.** Os valores de `EQU` sao guardados como constantes.
   `IF valor` e removido; se o valor for zero, a proxima linha nao vazia e
   descartada. Os numeros de linha originais continuam associados as linhas
   restantes.
3. **Analise.** Cada linha ativa vira uma instrucao ou diretiva. Rotulos em
   linhas isoladas ficam pendentes e sao associados a proxima operacao, o que
   permite que varios rotulos em linhas consecutivas apontem para o mesmo
   endereco. A analise acumula erros em vez de parar no primeiro.
4. **Passagem 1.** Sao calculados os enderecos e preenchida a tabela de
   simbolos.
5. **Passagem 2.** Sao resolvidos operandos e geradas as palavras de codigo.
6. **Gravacao.** O arquivo de saida so e aberto se a montagem terminou sem
   erros.

### Pre-processamento de `EQU` e `IF`

Uma declaracao como:

```asm
TAMANHO: EQU 5
```

nao ocupa memoria. O valor pode ser usado, por exemplo, em `SPACE TAMANHO`,
`CONST TAMANHO` ou `IF TAMANHO`. `EQU` precisa aparecer antes das secoes.

`IF` e permitido somente em `TEXT`:

```asm
SECAO TEXT
IF TAMANHO
LOAD DADO
```

Como `TAMANHO` nao e zero, `LOAD DADO` permanece no programa. Se a condicao
fosse zero, essa linha seria removida. Linhas em branco e comentarios nao
contam como a proxima linha a ser descartada.

### Passagem 1: enderecos e simbolos

O montador mantem dois contadores independentes:

- `text_lc`: soma o tamanho de cada instrucao em `TEXT`;
- `data_lc`: soma o tamanho de cada `CONST` ou `SPACE` em `DATA`.

Rotulos de `TEXT` recebem o valor atual de `text_lc`. Rotulos de `DATA` guardam
inicialmente seu deslocamento dentro dos dados. Quando o tamanho do texto e
conhecido, os enderecos dos dados passam a ser `text_size + deslocamento`.
Assim, os dados ficam depois de todas as instrucoes mesmo que a secao `DATA`
apareca primeiro no fonte.

Exemplo:

```asm
SECAO TEXT
LOAD VALOR
STOP
SECAO DATA
VALOR: CONST 7
```

`LOAD` ocupa duas palavras e `STOP` uma, entao `VALOR` fica no endereco 3. O
codigo gerado e:

```text
10 3 14 7
```

Cada rotulo so pode ser definido uma vez. `PUBLIC` nao cria um rotulo: ele
marca como exportado um simbolo definido localmente.

### Passagem 2: palavras de codigo

As instrucoes sao emitidas primeiro, na ordem em que aparecem na secao `TEXT`.
Em seguida sao emitidos os dados, na ordem em que aparecem na secao `DATA`:

- `CONST valor` gera uma palavra com o valor;
- `SPACE` gera um zero;
- `SPACE N` gera `N` zeros.

Um operando local como `DADO + 2` vira o endereco de `DADO` somado a dois.
Rotulos nao declarados geram erro semantico.

## 4. Objetos e relocacao

Um fonte sem marcadores de modulo produz um `.obj` com uma unica linha de
palavras decimais separadas por um espaco. A linha contem texto e dados, sem
separador visivel.

Um fonte com `BEGIN`, `PUBLIC` ou `EXTERN` e tratado como modulo. Seu `.obj`
contem tabelas antes do codigo:

```text
D, SIMBOLO endereco
U, EXTERNO posicao
R, bit bit bit ...
palavras do modulo
```

- **`D` (definicoes):** cada simbolo `PUBLIC` e associado ao seu endereco
  local no modulo.
- **`U` (usos):** cada ocorrencia de um simbolo `EXTERN` usado registra a
  posicao da palavra que precisa ser corrigida. Nessa palavra fica somente o
  deslocamento do operando.
- **`R` (relocacao):** existe um bit por palavra de codigo. O bit e `1` quando
  a palavra contem um endereco relativo a simbolo local; e `0` para opcodes,
  constantes, espacos e posicoes de simbolos externos.

Por exemplo, um operando local gera uma palavra marcada com `R=1`; o mesmo
operando declarado como `EXTERN` gera uma entrada `U` e mantem `R=0`. O codigo
de modulo continua organizado como texto seguido pelos dados.

## 5. Fluxo do ligador

O ligador monta cada arquivo recebido e aborta a ligacao se algum deles tiver
erros. Depois:

1. Calcula o fator de correcao de cada modulo. O primeiro fator e zero; os
   seguintes sao a soma dos tamanhos dos modulos anteriores.
2. Constroi a tabela global de definicoes. Um endereco `D` local e somado ao
   fator do modulo que o definiu.
3. Verifica simbolos `PUBLIC` duplicados e simbolos `EXTERN` sem definicao.
4. Para cada uso `U`, soma ao deslocamento o endereco global da definicao.
5. Para cada palavra marcada com `R=1`, soma o fator do proprio modulo.
6. Verifica que os enderecos corrigidos estao dentro do executavel ligado e
   grava as palavras dos modulos concatenadas, na ordem dos argumentos.

Cada modulo ja inclui seu proprio texto e dados; portanto, o fator do proximo
modulo considera o tamanho total do modulo anterior.

O `.lig` final contem somente uma linha de palavras decimais. As tabelas `D`,
`U` e `R` aparecem nos objetos dos modulos, nao no executavel ligado.

## 6. Erros

Os erros do montador usam o formato:

```text
ERRO TIPO (linha N): descricao
```

Os tipos sao `LEXICO`, `SINTATICO` e `SEMANTICO`. A linha e a linha original
do arquivo-fonte. Exemplos de situacoes detectadas:

- token ou numero malformado: erro lexico;
- operacao inexistente ou quantidade incorreta de operandos: erro sintatico;
- rotulo duplicado ou nao declarado: erro semantico;
- instrucao fora de `TEXT`, dado fora de `DATA` ou ausencia de `TEXT`: erro
  semantico.

O montador percorre o arquivo para acumular varios erros na mesma execucao.
Erros do ligador, como importacao sem definicao ou exportacao duplicada, sao
apresentados como erros semanticos.

## 7. Decisoes implementadas

- As extensoes de saida sao `.obj` e `.lig`, definidas por `OBJ_EXT` e
  `LIG_EXT` em `structures.h`.
- `MUL` e `MULT` representam o mesmo opcode.
- Uma virgula em `COPY` e rejeitada como erro sintatico; os operandos devem
  ser separados por espaco.
- Um arquivo e considerado modulo se possui `BEGIN`, `PUBLIC` ou `EXTERN`.
- `END` e aceito sem interromper a leitura do arquivo.
- A memoria aceita no maximo `2^16` palavras.
