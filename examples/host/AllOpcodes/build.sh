#!/bin/bash
# Build and test all_opcodes.ml through ocamlc → camelino-embed → host VM
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"
RUN="$ROOT/build/camelino-run"

echo "=== Camelino Host: All Opcodes ==="

# Build tools
if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi
if [ ! -x "$RUN" ] && [ ! -x "$RUN.exe" ]; then
    echo "Building camelino-run (cmake)..."
    cmake -G Ninja -B "$ROOT/build" -DCMAKE_C_COMPILER=gcc >/dev/null 2>&1
    cmake --build "$ROOT/build" 2>/dev/null
fi

# Use .exe if needed
[ -x "$RUN.exe" ] && RUN="$RUN.exe"

echo "Compiling all_opcodes.ml..."
ocamlc -c -o all_opcodes.cmo all_opcodes.ml 2>&1

echo "Embedding → all_opcodes.camel..."
"$EMBED" all_opcodes.cmo -o all_opcodes.camel 2>&1

echo "Running..."
"$RUN" all_opcodes.camel 2>&1

rm -f all_opcodes.cmo all_opcodes.camel all_opcodes.cmi
echo "=== Done ==="
