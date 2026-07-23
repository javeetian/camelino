#!/bin/bash
# ============================================================
# Hello — 主机端 Camelino 示例
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"

echo "=== Camelino Host: Hello ==="

# 1. Build camelino-embed if needed
if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi

# 2. OCaml → .camel
echo "Compiling hello.ml → hello.camel..."
ocamlc -c -o hello.cmo hello.ml
"$EMBED" hello.cmo -o hello.camel

# 3. Compile C driver + camelino runtime
echo "Building host binary..."
cat > _driver.c << 'DRIVER'
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
extern void caml_init_primitives(void);

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "hello.camel";
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    uint8_t* buf = malloc(sz);
    fread(buf, 1, sz, f); fclose(f);

    if (caml_bytecode_load(buf, sz) != CAML_BC_OK) {
        fprintf(stderr, "Bad .camel file\n"); return 1;
    }
    caml_vm_init();
    caml_init_primitives();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(),
                           caml_bytecode_data(), caml_bytecode_data_size());
    caml_init_globals();
    caml_interpret();

    caml_bytecode_unload(); free(buf);
    return 0;
}
DRIVER

gcc _driver.c \
    "$ROOT/src/core/vm.c" \
    "$ROOT/src/core/memory.c" \
    "$ROOT/src/core/value.c" \
    "$ROOT/src/core/error.c" \
    "$ROOT/src/core/bytecode.c" \
    "$ROOT/src/core/gc_roots.c" \
    "$ROOT/src/ffi/ffi.c" \
    -I"$ROOT/src/core" -I"$ROOT/src/ffi" -I"$ROOT/platform/host" \
    -std=gnu99 -Wall -o hello

rm -f _driver.c hello.cmo
echo "=== Done: ./hello ==="
