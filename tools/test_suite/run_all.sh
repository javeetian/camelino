#!/bin/bash
# Camelino Full Regression — run_all.sh (Phase 0-6)
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

PASSED=0; TOTAL=0
pass() { PASSED=$((PASSED+1)); TOTAL=$((TOTAL+1)); echo "  ✓ $1"; }
fail() { TOTAL=$((TOTAL+1)); echo "  ✗ FAIL: $1"; }

echo "╔══════════════════════════════════════════════════════╗"
echo "║     Camelino Full Regression (Phase 0-6)            ║"
echo "╚══════════════════════════════════════════════════════╝"

# ---- C Unit Tests ----
echo ""; echo "── C Unit Tests ───────────────────────────────────"
if ctest --test-dir build --output-on-failure 2>&1 | grep -q "100% tests"; then
    pass "ctest (14/14)"
else
    fail "ctest"
fi

# ---- Quick Diff Tests ----
echo ""; echo "── Quick Diff Tests ───────────────────────────────"
if bash tools/test_suite/run_diff.sh 2>&1 | grep -q "0 failed"; then
    pass "run_diff.sh (3/3)"
else
    fail "run_diff.sh"
fi

# ---- Full Diff Tests ----
echo ""; echo "── Full Diff Tests ────────────────────────────────"
if bash tools/test_suite/run_diff_full.sh 2>&1 | grep -q "0 failed"; then
    pass "run_diff_full.sh (41/41)"
else
    fail "run_diff_full.sh"
fi

# ---- Syntax Check ----
echo ""; echo "── Syntax Check ───────────────────────────────────"
SYN_OK=0; SYN_FAIL=0
for f in src/core/*.c src/ffi/*.c src/bindings/*.c src/repl/*.c test/test_*.c; do
    if gcc -fsyntax-only "$f" -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi -std=gnu99 2>/dev/null; then
        SYN_OK=$((SYN_OK+1))
    else
        echo "    FAIL: $f"; SYN_FAIL=$((SYN_FAIL+1))
    fi
done
if [ $SYN_FAIL -eq 0 ]; then
    pass "syntax ($SYN_OK files OK)"
else
    fail "syntax ($SYN_FAIL files failed)"
fi

# ---- OCaml Tools ----
echo ""; echo "── OCaml Tools ────────────────────────────────────"
if (cd tools/camelino-embed && ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml 2>/dev/null); then
    pass "camelino-embed build"
else
    fail "camelino-embed build"
fi

if (cd tools/camelino-check && dune build main.exe 2>/dev/null); then
    pass "camelino-check build"
else
    fail "camelino-check build"
fi

if (cd tools/camelino-repl && dune build main.exe 2>/dev/null); then
    pass "camelino-repl build"
else
    fail "camelino-repl build"
fi

# ---- camelino-check smoke test ----
echo ""; echo "── camelino-check smoke test ─────────────────────"
echo 'let _ = 2 + 3' > /tmp/cml_reg.ml
ocamlc -c -o /tmp/cml_reg.cmo /tmp/cml_reg.ml 2>/dev/null
if tools/camelino-check/_build/default/main.exe /tmp/cml_reg.cmo --heap 192k --word-size 32 --platform arduino 2>&1 | grep -q "READY"; then
    pass "camelino-check functional"
else
    fail "camelino-check functional"
fi
rm -f /tmp/cml_reg.ml /tmp/cml_reg.cmo

# ---- camelino-stub scan ----
echo ""; echo "── camelino-stub scan ────────────────────────────"
if bash tools/camelino-stub.sh scan tools/test_stub.c 2>&1 | grep -q "caml_test_add"; then
    pass "camelino-stub scan"
else
    fail "camelino-stub scan"
fi

# ---- Summary ----
echo ""
echo "═══════════════════════════════════════════════════════"
echo "  $PASSED / $TOTAL passed"
echo "═══════════════════════════════════════════════════════"
[ "$PASSED" -eq "$TOTAL" ] && exit 0 || exit 1
