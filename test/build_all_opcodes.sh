#!/bin/bash
# Build and run the full opcode test suite
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"

echo "=== Camelino: Full Opcode Test ==="

# Compile as single translation unit with all core sources
COMBINED="/tmp/test_all_combined.c"
cat test_all.c \
    "$ROOT/src/core/vm.c" \
    "$ROOT/src/core/memory.c" \
    "$ROOT/src/core/value.c" \
    "$ROOT/src/core/error.c" \
    "$ROOT/src/core/gc_roots.c" \
    "$ROOT/src/ffi/ffi.c" \
    "$ROOT/src/ffi/primitives.c" \
    "$ROOT/platform/host/hal_adapter.c" \
    > "$COMBINED"

gcc "$COMBINED" \
    -I"$ROOT/src/core" -I"$ROOT/platform/host" -I"$ROOT/src" \
    -I"$ROOT/src/hal" -I"$ROOT/src/ffi" \
    -std=gnu99 -DCAMELINO_HAS_FFI \
    -Wall -Wno-unused-variable -Wno-unused-function \
    -Wno-typedef-redefinition -Wno-misleading-indentation \
    -o test_all 2>/dev/null

rm -f "$COMBINED"

echo "Running..."
./test_all
RC=$?
rm -f test_all
exit $RC
