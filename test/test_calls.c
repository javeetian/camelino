/**
 * @file test/test_calls.c
 * @brief Phase 2.x 闭包 + 函数调用测试
 *
 * 4.14 closure: field0=code_ptr, field1=closinfo(Val_long(arity)), field2+=free vars
 * Stack: args bottom, return frame (3 slots) on top. Function sees arg0 at sp[3].
 */
#include <stdio.h>
#include <assert.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/opcodes.h"

#define RUN_TEST(name) do{printf("  %-35s",#name);name();printf("OK\n");}while(0)
#define ASSERT(e) assert(e)

static void run(uint8_t* bc, size_t len) {
    caml_vm_init(); caml_load_bytecode_buf(bc, len, NULL, 0); caml_interpret();
}

/* ---- Test 1: f(x)=x+1, call f(5) → 6 ---- */
void test_apply1(void) {
    uint8_t bc[] = {
        CLOSURE, 17,0,0,0, 0,1, PUSH, CONSTINT, 5,0,0,0, PUSH, ACC1, APPLY1, STOP,
        ACC3, OFFSETINT, 1, RETURN, 1
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 6);
}

/* ---- Test 2: f(x)=x+1, call f(99) → 100 ---- */
void test_apply1_99(void) {
    uint8_t bc[] = {
        CLOSURE, 17,0,0,0, 0,1, PUSH, CONSTINT, 99,0,0,0, PUSH, ACC1, APPLY1, STOP,
        ACC3, OFFSETINT, 1, RETURN, 1
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 100);
}

/* ---- Test 3: f(x,y)=y+1, call f(3,4) via APPLY2 → 5 ---- */
void test_apply2_offsets(void) {
    /* CLOSURE(23, arity=2) → PUSH(f) → CONSTINT(3),PUSH(x) → CONSTINT(4),PUSH(y)
       → ACC2(f) → APPLY2 → STOP
       Function at 23: ACC3(y=4)+OFFSETINT(1)=5 → RETURN */
    uint8_t bc[] = {
        CLOSURE, 23,0,0,0, 0,2,
        PUSH, CONSTINT, 3,0,0,0, PUSH, CONSTINT, 4,0,0,0, PUSH,
        ACC2, APPLY2, STOP,
        ACC3, OFFSETINT, 1, RETURN, 1
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 5);
}

int main(void) {
    printf("=== Phase 2.x: Closure & Call Tests ===\n\n");
    RUN_TEST(test_apply1);
    RUN_TEST(test_apply1_99);
    RUN_TEST(test_apply2_offsets);
    printf("\n=== All %d tests passed! ===\n", 3);
    return 0;
}
