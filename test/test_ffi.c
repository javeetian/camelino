/**
 * @file test/test_ffi.c
 * @brief Phase 4.1 C_CALL* 调度测试
 */
#include <stdio.h>
#include <assert.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/opcodes.h"
#include "../src/ffi/ffi.h"

#define RUN_TEST(name) do{printf("  %-35s",#name);name();printf("OK\n");}while(0)
#define ASSERT(e) assert(e)

static void run(uint8_t* bc, size_t len) {
    caml_vm_init(); caml_init_primitives();
    caml_load_bytecode_buf(bc, len, NULL, 0); caml_interpret();
}

/* Test 1: C_CALL1 prim_identity(42) → 42 */
void test_ffi_identity(void) {
    /* prim_identity is index 0. C_CALL1(idx=0), arg=42 on stack, result in acc.
       Push 42, then C_CALL1 */
    uint8_t bc[] = {
        CONSTINT, 42,0,0,0,        /* acc = 42 */
        PUSH,                       /* push arg */
        C_CALL1, 0,0,0,0,          /* call primitive #0 */
        STOP
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 42);
}

/* Test 2: C_CALL1 prim_add1(5) → 6 */
void test_ffi_add1(void) {
    uint8_t bc[] = {
        CONSTINT, 5,0,0,0, PUSH,
        C_CALL1, 1,0,0,0,          /* primitive #1 = add1 */
        STOP
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 6);
}

int main(void) {
    printf("=== Phase 4.1: FFI Tests ===\n\n");
    RUN_TEST(test_ffi_identity);
    RUN_TEST(test_ffi_add1);
    printf("\n=== All %d tests passed! ===\n", 2);
    return 0;
}
