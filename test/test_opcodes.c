/**
 * @file test/test_opcodes.c
 * @brief opcodes.h 单元测试 — 验证指令值与 OCaml 4.14 官方对齐
 *
 * 交叉引用：ocaml/runtime/caml/instruct.h
 * 编译：cmake --build build && ctest --test-dir build
 */

#include <stdio.h>
#include <assert.h>
#include "../src/core/camelino_config.h"  /* must be before opcodes.h → byteorder.h */
#include "../src/core/opcodes.h"

#define TEST(name)     void name(void)
#define RUN_TEST(name) do { printf("  %-35s", #name); name(); printf("OK\n"); } while(0)
#define ASSERT(expr)   assert(expr)

/* 测试 1：指令总数 */
TEST(test_instruction_count) {
    ASSERT(CAML_NUM_INSTRUCTIONS == 149);
}

/* 测试 2：P0 核心指令值（与 OCaml 4.14 instruct.h 交叉验证） */
TEST(test_p0_core_values) {
    ASSERT(ADDINT == 110);
    ASSERT(SUBINT == 111);
    ASSERT(MULINT == 112);
    ASSERT(DIVINT == 113);
    ASSERT(MODINT == 114);
}

/* 测试 3：函数调用 */
TEST(test_function_call_ops) {
    ASSERT(PUSH_RETADDR == 31);
    ASSERT(APPLY == 32);
    ASSERT(RETURN == 40);
    ASSERT(GRAB == 42);
}

/* 测试 4：堆块 */
TEST(test_block_ops) {
    ASSERT(MAKEBLOCK == 62);
    ASSERT(GETFIELD == 71);
    ASSERT(SETFIELD == 77);
    ASSERT(GETVECTITEM == 80);
    ASSERT(SETVECTITEM == 81);
}

/* 测试 5：分支 */
TEST(test_branch_ops) {
    ASSERT(BRANCH == 84);
    ASSERT(BRANCHIF == 85);
    ASSERT(BRANCHIFNOT == 86);
}

/* 测试 6：异常 */
TEST(test_exception_ops) {
    ASSERT(PUSHTRAP == 89);
    ASSERT(POPTRAP == 90);
    ASSERT(RAISE == 91);
    ASSERT(RAISE_NOTRACE == 147);
}

/* 测试 7：FFI */
TEST(test_ffi_ops) {
    ASSERT(C_CALL1 == 93);
    ASSERT(C_CALLN == 98);
}

/* 测试 8：常量 */
TEST(test_const_ops) {
    ASSERT(CONST0 == 99);
    ASSERT(CONSTINT == 103);
    ASSERT(PUSHCONSTINT == 108);
}

/* 测试 9：控制流 */
TEST(test_control_ops) {
    ASSERT(STOP == 143);
    ASSERT(EVENT == 144);
    ASSERT(BREAK == 145);
}

/* 测试 10：对象 (P2) */
TEST(test_object_ops) {
    ASSERT(GETMETHOD == 130);
    ASSERT(GETPUBMET == 141);
    ASSERT(GETDYNMET == 142);
}

int main(void) {
    printf("=== Camelino Opcode Tests (vs OCaml 4.14 instruct.h) ===\n\n");

    RUN_TEST(test_instruction_count);
    RUN_TEST(test_p0_core_values);
    RUN_TEST(test_function_call_ops);
    RUN_TEST(test_block_ops);
    RUN_TEST(test_branch_ops);
    RUN_TEST(test_exception_ops);
    RUN_TEST(test_ffi_ops);
    RUN_TEST(test_const_ops);
    RUN_TEST(test_control_ops);
    RUN_TEST(test_object_ops);

    printf("\n=== All %d tests passed! ===\n", 10);
    return 0;
}
