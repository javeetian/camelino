#!/bin/bash
# Camelino Full Differential Test Suite (Phase 6.3)
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SRC="$ROOT/src"
PLATFORM="$ROOT/platform/host"

echo "=== Camelino Full Differential Test Suite ==="
echo ""

# Build the combined diff test binary
COMBINED="/tmp/camelino_diff_tests_$$.c"
EXE="/tmp/camelino_diff_tests_$$"

cat "$ROOT/tools/test_suite/diff_tests.c" \
    "$SRC/core/vm.c" \
    "$SRC/core/memory.c" \
    "$SRC/core/value.c" \
    "$SRC/core/error.c" \
    "$SRC/core/gc_roots.c" \
    "$SRC/ffi/ffi.c" \
    "$SRC/ffi/primitives.c" \
    "$PLATFORM/hal_adapter.c" \
    > "$COMBINED"

gcc "$COMBINED" \
    -I"$SRC/core" -I"$PLATFORM" -I"$SRC" -I"$SRC/hal" -I"$SRC/ffi" \
    -std=gnu99 -DCAMELINO_HAS_FFI \
    -Wall -Wno-unused-variable -Wno-unused-function \
    -Wno-typedef-redefinition -Wno-misleading-indentation \
    -o "$EXE" 2>/dev/null

"$EXE"
RC=$?

rm -f "$COMBINED" "$EXE"
exit $RC
