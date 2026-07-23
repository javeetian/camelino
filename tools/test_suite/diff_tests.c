/**
 * @file tools/test_suite/diff_tests.c
 * @brief Camelino Differential Test Suite — Phase 6.3
 *
 * 覆盖所有 VM 已实现的指令。编译和运行见 run_diff_full.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "memory.h"
#include "value.h"
#include "opcodes.h"

static int run=0, pass=0, fail=0;

static int64_t exec(const uint8_t* code, size_t len) {
    caml_vm_init();
    caml_load_bytecode_buf(code, len, NULL, 0);
    caml_interpret();
    return Long_val(caml_get_acc());
}

static void T(const char* name, const uint8_t* code, size_t len, int64_t expected) {
    run++;
    int64_t actual = exec(code, len);
    if (actual == expected) {
        printf("  PASS %s\n", name); pass++;
    } else {
        printf("  FAIL %s  (expected=%lld, actual=%lld)\n",
               name, (long long)expected, (long long)actual);
        fail++;
    }
}

static void w32(uint8_t* p, int32_t v) {
    p[0]=(uint8_t)(v&0xFF);p[1]=(uint8_t)((v>>8)&0xFF);
    p[2]=(uint8_t)((v>>16)&0xFF);p[3]=(uint8_t)((v>>24)&0xFF);
}

/* ---- Phase 1: Arithmetic (17 tests) ---- */
static void p1_arith(void) {
    printf("\n── Phase 1: Arithmetic (17 tests) ───────────────\n");
    {uint8_t c[]={CONST2,OFFSETINT,3,STOP};T("add_2_3",c,sizeof(c),5);}
    {uint8_t c[]={CONSTINT,10,0,0,0,OFFSETINT,(uint8_t)-3,STOP};T("sub_10_3",c,8,7);}
    {uint8_t c[]={CONSTINT,7,0,0,0,PUSH,CONSTINT,6,0,0,0,MULINT,STOP};T("mul_6_7",c,13,42);}
    {uint8_t c[]={CONST2,PUSH,CONSTINT,6,0,0,0,DIVINT,STOP};T("div_6_2",c,9,3);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,10,0,0,0,MODINT,STOP};T("mod_10_3",c,9,1);}
    {uint8_t c[]={CONSTINT,42,0,0,0,NEGINT,STOP};T("neg_42",c,7,-42);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,ANDINT,STOP};T("and_5_3",c,9,1);}
    {uint8_t c[]={CONST2,PUSH,CONSTINT,5,0,0,0,ORINT,STOP};T("or_5_2",c,9,7);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,XORINT,STOP};T("xor_5_3",c,9,6);}
    {uint8_t c[]={CONST3,PUSH,CONST1,LSLINT,STOP};T("lsl_1_3",c,5,8);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,8,0,0,0,LSRINT,STOP};T("lsr_8_3",c,9,1);}
    {uint8_t c[]={CONST0,STOP};T("const0",c,2,0);}
    {uint8_t c[]={CONST1,STOP};T("const1",c,2,1);}
    {uint8_t c[]={CONST2,STOP};T("const2",c,2,2);}
    {uint8_t c[]={CONST3,STOP};T("const3",c,2,3);}
    {uint8_t c[]={CONSTINT,57,48,0,0,STOP};T("constint_12345",c,6,12345);}
    {uint8_t c[]={CONSTINT,156,255,255,255,STOP};T("constint_neg100",c,6,-100);}
}

/* ---- Phase 1: Comparison (8 tests) ---- */
static void p1_cmp(void) {
    printf("\n── Phase 1: Comparison (8 tests) ────────────────\n");
    {uint8_t c[]={CONST3,PUSH,CONST3,EQ,STOP};T("eq_3_3_true",c,5,1);}
    {uint8_t c[]={CONSTINT,5,0,0,0,PUSH,CONST3,EQ,STOP};T("eq_3_neq_5",c,9,0);}
    {uint8_t c[]={CONSTINT,5,0,0,0,PUSH,CONST3,NEQ,STOP};T("neq_3_5_true",c,9,1);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,LTINT,STOP};T("lt_3_5_true",c,9,1);}
    {uint8_t c[]={CONSTINT,5,0,0,0,PUSH,CONST3,LTINT,STOP};T("lt_5_3_false",c,9,0);}
    {uint8_t c[]={CONST3,PUSH,CONST3,LEINT,STOP};T("le_3_3_true",c,5,1);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,GTINT,STOP};T("gt_3_5_false",c,9,0);}
    {uint8_t c[]={CONST3,PUSH,CONST3,GEINT,STOP};T("ge_3_3_true",c,5,1);}
}

/* ---- Phase 1: Branch (3 tests) ---- */
static void p1_branch(void) {
    printf("\n── Phase 1: Branch (3 tests) ────────────────────\n");
    {uint8_t c[]={CONST1,BRANCH,7,0,0,0,CONST3,STOP};T("branch_skip",c,8,1);}
    {uint8_t c[]={CONST1,BRANCHIF,7,0,0,0,CONST3,STOP};T("branchif_true",c,8,1);}
    {uint8_t c[]={CONST0,BRANCHIFNOT,7,0,0,0,CONST3,STOP};T("branchifnot_false",c,8,0);}
}

/* ---- Phase 1: Stack (2 tests) ---- */
static void p1_stack(void) {
    printf("\n── Phase 1: Stack (2 tests) ─────────────────────\n");
    {uint8_t c[]={CONST3,PUSH,CONST0,ASSIGN,1,ACC1,STOP};T("assign_sp1",c,7,0);}
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,ACC0,STOP};T("push_then_acc0",c,9,3);}
}

/* ---- Phase 2: Globals (2 tests) ---- */
static void p2_globals(void) {
    printf("\n── Phase 2: Globals (2 tests) ───────────────────\n");
    {uint8_t c[]={CONSTINT,42,0,0,0,SETGLOBAL,0,0,0,0,GETGLOBAL,0,0,0,0,STOP};
     T("set_get_global",c,16,42);}
    {uint8_t c[]={CONSTINT,42,0,0,0,SETGLOBAL,0,0,0,0,
                  CONSTINT,99,0,0,0,SETGLOBAL,0,0,0,0,
                  GETGLOBAL,0,0,0,0,STOP};
     T("set_get_global_overwrite",c,27,99);}
}

/* ---- Phase 2: Exception (2 tests) ---- */
static void p2_exn(void) {
    printf("\n── Phase 2: Exception (2 tests) ────────────────\n");
    /* PUSHTRAP: handler at offset 9 from pos5. RAISE at pos6.
       pc after PUSHTRAP operand = 5. handler = 5+9 = 14. CONSTINT at 14, STOP at 19. */
    {uint8_t c[]={PUSHTRAP,9,0,0,0,               /* 0-4: handler = pc_at_5 + 9 = 14 */
                  CONST3,                           /* 5: acc=3 */
                  RAISE,                            /* 6: jump to handler at 14 */
                  CONST0,STOP,CONST0,STOP,          /* 7-10: dead */
                  CONST0,STOP,CONST0,               /* 11-13: dead */
                  CONSTINT,42,0,0,0,                /* 14-18: handler: acc=42 */
                  STOP};                            /* 19: halt */
     T("raise_caught_42",c,sizeof(c),42);}
    /* Uncaught raise → VM halts */
    {uint8_t c[]={CONST3,RAISE,CONST0,STOP};
     caml_vm_init();caml_load_bytecode_buf(c,sizeof(c),NULL,0);caml_interpret();
     run++;
     if(caml_vm_halted()){printf("  PASS raise_uncaught_halt\n");pass++;}
     else{printf("  FAIL raise_uncaught_halt\n");fail++;}
    }
}

/* ---- Phase 5: P1 Instructions (6 tests) ---- */
static void p5_p1(void) {
    printf("\n── Phase 5: P1 Instructions (6 tests) ───────────\n");
    {uint8_t c[]={CONST0,BOOLNOT,STOP};T("boolnot_false",c,3,1);}
    {uint8_t c[]={CONST1,BOOLNOT,STOP};T("boolnot_true",c,3,0);}
    {uint8_t c[]={CONST3,ISINT,STOP};T("isint_3",c,3,1);}
    {uint8_t c[]={CONSTINT,255,255,255,255,PUSH,CONST1,ULTINT,STOP};
     T("ultint_minus1_lt_1",c,9,0);}
    {uint8_t c[]={CONST3,PUSH,CONST3,BEQ,9,0,0,0,CONST0,STOP};T("beq_equal_skip",c,10,3);}
    {uint8_t c[]={CONSTINT,5,0,0,0,PUSH,CONST3,BNEQ,13,0,0,0,CONST0,STOP};T("bneq_3_5_skip",c,14,3);}
}

/* ---- Phase 4: FFI (1 test) ---- */
extern void caml_init_primitives(void);
static void p4_ffi(void) {
    printf("\n── Phase 4: FFI (1 test) ────────────────────────\n");
    {uint8_t c[]={CONSTINT,42,0,0,0,PUSH,C_CALL1,0,0,0,0,STOP};
     caml_vm_init();caml_init_primitives();
     caml_load_bytecode_buf(c,sizeof(c),NULL,0);caml_interpret();
     run++;
     int64_t actual=Long_val(caml_get_acc());
     if(actual==42){printf("  PASS ffi_identity_42\n");pass++;}
     else{printf("  FAIL ffi_identity_42 (expected=42, actual=%lld)\n",(long long)actual);fail++;}
    }
}

/* ---- Main ---- */
int main(void) {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║   Camelino Diff Test Suite (Phase 6.3)              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    p1_arith();
    p1_cmp();
    p1_branch();
    p1_stack();
    p2_globals();
    p2_exn();
    p4_ffi();
    p5_p1();
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("  %d passed, %d failed (out of %d)\n", pass, fail, run);
    printf("═══════════════════════════════════════════════════════\n");
    return fail > 0 ? 1 : 0;
}
