#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"

echo "=== Camelino SerialHello: .ml → bytecode.h ==="

if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi

echo "Compiling serial_hello.ml → serial_hello.cmo..."
ocamlc -I "$ROOT/lib/camelino" -c -o serial_hello.cmo serial_hello.ml

echo "Embedding → bytecode.h..."
"$EMBED" serial_hello.cmo -o bytecode.h

echo "=== Done: bytecode.h generated ==="
