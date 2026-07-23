#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"
EMBED="$ROOT/tools/camelino-embed/camelino-embed"

echo "=== Camelino AnalogRead: .ml → bytecode.h ==="

if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi

echo "Compiling analog_read.ml → analog_read.cmo..."
ocamlc -I "$ROOT/lib/camelino" -c -o analog_read.cmo analog_read.ml

echo "Embedding → bytecode.h..."
"$EMBED" analog_read.cmo -o bytecode.h

echo "=== Done: bytecode.h generated ==="
