#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"
RUN="$ROOT/build/camelino-run.exe"

echo "=== Camelino Host: Hello ==="

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

echo "Compiling hello.ml → hello.camel..."
ocamlc -c -o hello.cmo hello.ml
"$EMBED" hello.cmo -o hello.camel

echo "Running..."
"$RUN" hello.camel

rm -f hello.cmo hello.camel hello.cmi
echo "=== Done ==="
