#!/bin/bash
# ============================================================
# Camelino 差分测试 (Phase 1)
# 用法：cd tools/test_suite && bash run_diff.sh
# ============================================================
set -o pipefail
ROOT="$(cd ../.. && pwd)"
PASS=0; FAIL=0
echo "=== Camelino Differential Tests (Phase 1) ==="
echo ""

run_one() {
  local name="$1" expect="$2" bc="$3"
  cat > _c.c << CODE
#include <stdio.h>
#include "platform.h"
#include "camelino_config.h"
#include "value.h"
#include "vm.h"
#include "memory.h"
int main(void){uint8_t b[]={$bc};caml_vm_init();caml_load_bytecode_buf(b,sizeof(b),NULL,0);caml_interpret();printf("%ld\\n",(long)Long_val(caml_get_acc()));return 0;}
CODE
  cat _c.c "$ROOT/src/core/vm.c" "$ROOT/src/core/memory.c" "$ROOT/src/core/value.c" "$ROOT/src/core/error.c" "$ROOT/src/core/gc_roots.c" > _comb.c
  gcc _comb.c -I"$ROOT/src/core" -I"$ROOT/platform/host" -I"$ROOT/src" -std=gnu99 -o _r.exe 2>/dev/null
  actual=$(./_r.exe 2>/dev/null | tr -d ' \n\r')
  if [ "$actual" = "$expect" ]; then
    echo "  PASS $name (expected=$expect)"; PASS=$((PASS+1))
  else
    echo "  FAIL $name (expected=$expect, actual=$actual)"; FAIL=$((FAIL+1))
  fi
  rm -f _c.c _comb.c _r.exe
}

# case01: 2+3 → CONST2, OFFSETINT(3), ATOM0, SETGLOBAL(0), STOP
run_one "case01_add (2+3)" 5 "0x65,0x7f,0x03,0x3a,0x39,0x00,0x00,0x00,0x8f"

# case02: 10-3 → CONSTINT(10), OFFSETINT(-3), ATOM0, SETGLOBAL(0), STOP
run_one "case02_sub (10-3)" 7 "0x67,0x0a,0x00,0x00,0x00,0x7f,0xfd,0x3a,0x39,0x00,0x00,0x00,0x8f"

# case03: 6*7 → CONSTINT(7), PUSHCONSTINT(6), MULINT, ATOM0, SETGLOBAL(0), STOP
run_one "case03_mul (6*7)" 42 "0x67,0x07,0x00,0x00,0x00,0x6c,0x06,0x00,0x00,0x00,0x70,0x3a,0x39,0x00,0x00,0x00,0x8f"

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -gt 0 ] && exit 1
exit 0
