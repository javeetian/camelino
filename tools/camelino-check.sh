#!/bin/bash
# camelino-check — wrapper script for OCaml bytecode static analysis
# Usage: camelino-check [options] input.cmo

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXE="$SCRIPT_DIR/camelino-check/_build/default/main.exe"

if [ ! -f "$EXE" ]; then
    echo "Building camelino-check..."
    cd "$SCRIPT_DIR/camelino-check" && dune build || exit 1
fi

exec "$EXE" "$@"
