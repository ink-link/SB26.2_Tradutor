#!/usr/bin/env bash
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
make -C "$root" all
tmp="$(mktemp -d)"
passed=0
failed=0
trap 'rm -rf "$tmp"' EXIT

pass() {
    printf 'PASS %s\n' "$1"
    passed=$((passed + 1))
}

fail() {
    printf 'FAIL %s\n' "$1"
    failed=$((failed + 1))
}

check_output() {
    name="$1"
    actual="$2"
    expected="$3"
    expected_file="$tmp/$name.expected"
    printf '%s\n' "$expected" > "$expected_file"
    expected_from_file="$(read_output "$expected_file")"
    if [ "$actual" = "$expected_from_file" ]; then
        pass "$name"
    else
        printf '  expected: %s\n  actual:   %s\n' "$expected_from_file" "$actual"
        fail "$name"
    fi
}

read_output() {
    tr -d '\r' < "$1"
}

cat > "$tmp/t1.asm" <<'EOF'
SECTION TEXT
INPUT OLD_DATA
LOAD OLD_DATA
L1: DIV DOIS
STORE NEW_DATA
MULT DOIS
STORE TMP_DATA
LOAD OLD_DATA
SUB TMP_DATA
STORE TMP_DATA
OUTPUT TMP_DATA
COPY NEW_DATA OLD_DATA
LOAD OLD_DATA
JMPP L1
STOP
SECTION DATA
DOIS: CONST 2
OLD_DATA: SPACE
NEW_DATA: SPACE
TMP_DATA: SPACE
EOF
"$root/montador" "$tmp/t1.asm" >/dev/null
check_output "T1 simple assembly" "$(read_output "$tmp/t1.obj")" \
    "12 29 10 29 4 28 11 30 3 28 11 31 10 29 2 31 11 31 13 31 9 30 29 10 29 7 4 14 2 0 0 0"

cat > "$tmp/t2.asm" <<'EOF'
SECAO DATA
N1: SPACE
N2: CONST -48
N4: SPACE
SECAO TEXT
LOAD N1
STOP
EOF
"$root/montador" "$tmp/t2.asm" >/dev/null
check_output "T2 data before text" "$(read_output "$tmp/t2.obj")" "10 3 14 0 -48 0"

cat > "$tmp/t3.asm" <<'EOF'
SECTION TEXT
MOD1: BEGIN
MOD2: EXTERN
PUBLIC L2
PUBLIC OLD_DATA
PUBLIC NEW_DATA
PUBLIC TMP_DATA
INPUT OLD_DATA
LOAD OLD_DATA
JMP MOD2
L2: STOP
END
SECTION DATA
DOIS: CONST 2
OLD_DATA: SPACE
NEW_DATA: SPACE
TMP_DATA: SPACE
EOF
"$root/montador" "$tmp/t3.asm" >/dev/null
check_output "T3 module object" "$(read_output "$tmp/t3.obj")" \
    "$(printf 'D, L2 6\nD, OLD_DATA 8\nD, NEW_DATA 9\nD, TMP_DATA 10\nU, MOD2 5\nR, 0 1 0 1 0 0 0 0 0 0 0\n12 8 10 8 5 0 14 2 0 0 0')"

cat > "$tmp/a.asm" <<'EOF'
SECAO TEXT
MOD_A: BEGIN
Y: EXTERN
MOD_B: EXTERN
PUBLIC VAL
PUBLIC L1
INPUT Y
LOAD VAL
ADD Y
STORE Y + 2
JMPP MOD_B
L1: STOP
SECAO DATA
VAL: CONST 5
END
EOF
cat > "$tmp/b.asm" <<'EOF'
SECAO TEXT
MOD_B: BEGIN
VAL: EXTERN
L1: EXTERN
PUBLIC Y
PUBLIC MOD_B
OUTPUT Y
OUTPUT VAL
OUTPUT Y + 2
JMP L1
SECAO DATA
Y: SPACE 3
END
EOF
"$root/ligador" "$tmp/a.asm" "$tmp/b.asm" >/dev/null
check_output "T4 linking" "$(read_output "$tmp/a.lig")" \
    "12 20 10 11 1 20 11 22 7 12 14 5 13 20 13 11 13 22 5 10 0 0 0"

cat > "$tmp/t6.asm" <<'EOF'
; comentario
uM: eQu 1
zero: EQU 0
section	text ; comentario final
IF zero
LOAD MISSING
IF UM
LOAD
X: ; rotulo pendente
SECTION DATA
space 5
EOF
# This malformed fixture is separately used to verify diagnostics below.
if "$root/montador" "$tmp/t6.asm" > "$tmp/t6.errors"; then
    fail "T5 errors reject output"
else
    if grep -q 'ERRO SINTATICO (linha 8)' "$tmp/t6.errors" &&
       [ ! -e "$tmp/t6.obj" ]; then
        pass "T5 errors reject output"
    else
        fail "T5 errors reject output"
    fi
fi

cat > "$tmp/multiple-errors.asm" <<'EOF'
SECTION TEXT
A: STOP
A: STOP
LOAD MISSING
FOO X
COPY A
STOP X
SECTION DATA
BAD: CONST 0x10
1ABC: SPACE
EOF
if "$root/montador" "$tmp/multiple-errors.asm" > "$tmp/multiple-errors.out"; then
    fail "T5 collects assembler errors"
else
    if [ "$(grep -c '^ERRO ' "$tmp/multiple-errors.out")" -eq 7 ] &&
       grep -q '^ERRO LEXICO ' "$tmp/multiple-errors.out" &&
       grep -q '^ERRO SINTATICO ' "$tmp/multiple-errors.out" &&
       grep -q '^ERRO SEMANTICO ' "$tmp/multiple-errors.out" &&
       [ ! -e "$tmp/multiple-errors.obj" ]; then
        pass "T5 collects assembler errors"
    else
        cat "$tmp/multiple-errors.out"
        fail "T5 collects assembler errors"
    fi
fi

cat > "$tmp/unresolved-a.asm" <<'EOF'
SECTION TEXT
MOD_A: BEGIN
NOT_DEFINED: EXTERN
STOP
END
EOF
cat > "$tmp/unresolved-b.asm" <<'EOF'
SECTION TEXT
MOD_B: BEGIN
STOP
END
EOF
if "$root/ligador" "$tmp/unresolved-a.asm" "$tmp/unresolved-b.asm" \
    > "$tmp/link.errors"; then
    fail "T5 unresolved EXTERN"
else
    if grep -q "EXTERN 'NOT_DEFINED' nao definido" "$tmp/link.errors" &&
       [ ! -e "$tmp/unresolved-a.lig" ]; then
        pass "T5 unresolved EXTERN"
    else
        fail "T5 unresolved EXTERN"
    fi
fi

cat > "$tmp/duplicate-a.asm" <<'EOF'
SECTION TEXT
MOD_A: BEGIN
PUBLIC SHARED
SHARED: STOP
END
EOF
cat > "$tmp/duplicate-b.asm" <<'EOF'
SECTION TEXT
MOD_B: BEGIN
PUBLIC SHARED
SHARED: STOP
END
EOF
if "$root/ligador" "$tmp/duplicate-a.asm" "$tmp/duplicate-b.asm" \
    > "$tmp/duplicate.errors"; then
    fail "T5 duplicate PUBLIC"
else
    if grep -q "PUBLIC 'SHARED' definido em mais de um modulo" \
        "$tmp/duplicate.errors" && [ ! -e "$tmp/duplicate-a.lig" ]; then
        pass "T5 duplicate PUBLIC"
    else
        cat "$tmp/duplicate.errors"
        fail "T5 duplicate PUBLIC"
    fi
fi

cat > "$tmp/robust.asm" <<'EOF'
; comentario isolado
uM: eQu 1
zero: EQU 0
section	text ; comentario final
IF zero
LOAD MISSING
IF UM
L:
LOAD D
STOP
SECTION DATA
D: SPACE 5
EOF
"$root/montador" "$tmp/robust.asm" >/dev/null
check_output "T6 preprocessing and syntax" "$(read_output "$tmp/robust.obj")" \
    "10 3 14 0 0 0 0 0"

printf '%d passed, %d failed\n' "$passed" "$failed"
[ "$failed" -eq 0 ]
