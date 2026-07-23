#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"
RUN="$ROOT/build/camelino-run.exe"

echo "=== Camelino Host: Fib ==="

# Build tools if needed
if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi
if [ ! -x "$RUN" ]; then
    echo "Building camelino-run (cmake)..."
    cmake -G "Unix Makefiles" -B "$ROOT/build" -DCMAKE_C_COMPILER=gcc >/dev/null 2>&1
    cmake --build "$ROOT/build" 2>/dev/null
fi

echo "Compiling fib.ml → fib.camel..."
ocamlc -c -o fib.cmo fib.ml
"$EMBED" fib.cmo -o fib.camel

echo "Running..."
"$RUN" fib.camel

rm -f fib.cmo fib.camel
echo "=== Done ==="
