/**
 * @file test/test_vm.c
 * @brief ZAM 虚拟机单元测试 — Phase 1.4 P0 指令
 *
 * 手工构造字节码，喂给 caml_interpret()，验证 acc 计算结果。
 * 所有字节码以 OCaml 4.14 interp.c 的指令编码为准。
 *
 * 编译：cmake --build build
 * 运行：./build/test_vm   或   ctest --test-dir build -R test_vm
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/opcodes.h"

#define TEST(name)     void name(void)
#define RUN_TEST(name) do { printf("  %-35s", #name); name(); printf("OK\n"); } while(0)
#define ASSERT(expr)   assert(expr)

/* 辅助：初始化并加载字节码 */
static void load_and_run(const uint8_t* code, size_t len) {
    caml_vm_init();
    caml_load_bytecode_buf(code, len, NULL, 0);
    caml_interpret();
}

/* ---- 测试 1: CONST + STOP ---- */

TEST(test_const_stop) {
    /* CONST1 (0x64), STOP (0x8F) → acc = tagged(1) = 3 */
    uint8_t bc[] = { CONST1, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);
}

/* ---- 测试 2: 2 + 3 = 5 ---- */

TEST(test_add_2_3) {
    /* CONST3 → acc=3, PUSH → stack[0]=3, CONST2 → acc=2, ADDINT → 2+pop(3)=5 */
    uint8_t bc[] = { CONST3, PUSH, CONST2, ADDINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 5);
}

/* ---- 测试 3: CONSTINT（32-bit 立即数）---- */

TEST(test_constint_100) {
    uint8_t bc[] = { CONSTINT, 100, 0, 0, 0, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 100);
}

/* ---- 测试 4: 10 - 3 = 7 ---- */

TEST(test_sub_10_3) {
    /* CONST3 → acc=3, PUSH, CONSTINT(10) → acc=10, SUBINT → tagged(10)-tagged(3)+1=tagged(7) */
    uint8_t bc[] = { CONST3, PUSH, CONSTINT, 10,0,0,0, SUBINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 7);
}

/* ---- 测试 5: 6 * 7 = 42 ---- */

TEST(test_mul_6_7) {
    uint8_t bc[] = { CONSTINT, 7,0,0,0, PUSH, CONSTINT, 6,0,0,0, MULINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 42);
}

/* ---- 测试 6: 6 / 2 = 3 ---- */

TEST(test_div_6_2) {
    uint8_t bc[] = { CONST2, PUSH, CONSTINT, 6,0,0,0, DIVINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 3);
}

/* ---- 测试 7: 10 % 3 = 1 ---- */

TEST(test_mod_10_3) {
    uint8_t bc[] = { CONST3, PUSH, CONSTINT, 10,0,0,0, MODINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);
}

/* ---- 测试 8: 比较（5 > 3?）---- */

TEST(test_cmp_gt) {
    /* PUSH 5 (left), acc=3 (right), GTINT → left(5) > right(3) → true */
    uint8_t bc[] = { CONSTINT, 5,0,0,0, PUSH, CONST3, GTINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(caml_get_acc() == Val_true);
}

/* ---- 测试 9: 比较（3 == 3?）---- */

TEST(test_cmp_eq) {
    uint8_t bc[] = { CONST3, PUSH, CONST3, EQ, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(caml_get_acc() == Val_true);
}

/* ---- 测试 10: 比较（3 != 5?）---- */

TEST(test_cmp_neq) {
    uint8_t bc[] = { CONSTINT, 5,0,0,0, PUSH, CONST3, NEQ, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(caml_get_acc() == Val_true);   /* 3 != 5 → true */
}

/* ---- 测试 11: BRANCH 无条件跳转 ---- */

TEST(test_branch) {
    /* CONST1, BRANCH(offset=7=skip CONST3+STOP), CONST3, STOP, STOP */
    uint8_t bc[] = { CONST1, BRANCH, 7,0,0,0, CONST3, STOP, STOP };
    /*                                        ^^^^ offset=5: skip 1+1 bytes = CONST3+deadSTOP */
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);  /* CONST1 path */
}

/* ---- 测试 12: BRANCHIF（条件为真时跳转）---- */

TEST(test_branchif_true) {
    /* CONST1(1=true), BRANCHIF(offset=7=skip CONST3+STOP), CONST3, STOP, STOP */
    uint8_t bc[] = { CONST1, BRANCHIF, 7,0,0,0, CONST3, STOP, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);  /* jumped over CONST3 */
}

/* ---- 测试 13: BRANCHIFNOT（条件为假时跳转）---- */

TEST(test_branchifnot_false) {
    /* CONST0(0=false), BRANCHIFNOT(offset=7=skip CONST3+STOP), CONST3, STOP, STOP */
    uint8_t bc[] = { CONST0, BRANCHIFNOT, 7,0,0,0, CONST3, STOP, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 0);  /* jumped over CONST3 */
}

/* ---- 测试 14: STOP halts VM ---- */

TEST(test_stop_halt) {
    uint8_t bc[] = { CONST1, STOP, CONST3 };
    load_and_run(bc, sizeof(bc));
    ASSERT(caml_vm_halted() == 1);
    ASSERT(Long_val(caml_get_acc()) == 1);  /* stopped before CONST3 */
}

/* ---- 测试 15: PUSH + ACC + ADDINT（堆栈测试）---- */

TEST(test_stack_arith) {
    /* 手工模拟 let x=2 in let y=3 in x+y:
       CONST2, PUSH, CONST3, ADDINT, STOP
       acc = 3(推入栈) → acc = 2 → ADDINT = 2+3 = 5 */
    uint8_t bc[] = { CONST3, PUSH, CONST2, ADDINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 5);
}

/* ---- 测试 16: NEGINT ---- */

TEST(test_negint) {
    uint8_t bc[] = { CONSTINT, 42,0,0,0, NEGINT, STOP };
    load_and_run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == -42);
}

/* ---- 测试 17: 位运算 ---- */

TEST(test_bitwise) {
    /* 6 & 3 = 2 (push 3, acc=6, and) */
    uint8_t bc_and[] = { CONST3, PUSH, CONSTINT, 6,0,0,0, ANDINT, STOP };
    load_and_run(bc_and, sizeof(bc_and));
    ASSERT(Long_val(caml_get_acc()) == 2);

    /* 6 | 3 = 7 */
    uint8_t bc_or[] = { CONST3, PUSH, CONSTINT, 6,0,0,0, ORINT, STOP };
    load_and_run(bc_or, sizeof(bc_or));
    ASSERT(Long_val(caml_get_acc()) == 7);

    /* 6 ^ 3 = 5 */
    uint8_t bc_xor[] = { CONST3, PUSH, CONSTINT, 6,0,0,0, XORINT, STOP };
    load_and_run(bc_xor, sizeof(bc_xor));
    ASSERT(Long_val(caml_get_acc()) == 5);
}

int main(void) {
    printf("=== Camelino VM Tests (P0 Instructions) ===\n\n");

    RUN_TEST(test_const_stop);
    RUN_TEST(test_add_2_3);
    RUN_TEST(test_constint_100);
    RUN_TEST(test_sub_10_3);
    RUN_TEST(test_mul_6_7);
    RUN_TEST(test_div_6_2);
    RUN_TEST(test_mod_10_3);
    RUN_TEST(test_cmp_gt);
    RUN_TEST(test_cmp_eq);
    RUN_TEST(test_cmp_neq);
    RUN_TEST(test_branch);
    RUN_TEST(test_branchif_true);
    RUN_TEST(test_branchifnot_false);
    RUN_TEST(test_stop_halt);
    RUN_TEST(test_stack_arith);
    RUN_TEST(test_negint);
    RUN_TEST(test_bitwise);

    printf("\n=== All %d tests passed! ===\n", 17);
    return 0;
}
