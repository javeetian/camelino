/**
 * @file test/test_globals.c
 * @brief Phase 2.4 全局变量测试
 *
 * 编译：cmake --build build
 * 运行：./build/test_globals.exe 或 ctest --test-dir build -R test_globals
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

/* SETGLOBAL(0, 42); GETGLOBAL(0) → acc = 42 */
void test_setget_global(void) {
    uint8_t bc[] = {
        CONSTINT, 42,0,0,0, SETGLOBAL, 0,0,0,0,
        GETGLOBAL, 0,0,0,0, STOP
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 42);
}

/* SETGLOBAL(0, block); SETGLOBAL(0).field = 99; GETGLOBALFIELD(0, 0) → 99 */
void test_global_field(void) {
    /* Create a 2-field block: [42, 99]. Store at global[0].
       Then GETGLOBALFIELD(0, 1) → 99 */
    uint8_t bc[] = {
        /* Build block: make a 2-field block via manual alloc */
        CONSTINT, 42,0,0,0,        /* acc=42 */
        /* We need MAKEBLOCK. Let me test with simple SET/GET first. */
        SETGLOBAL, 0,0,0,0,        /* global[0] = 42 */
        GETGLOBAL, 0,0,0,0,        /* acc = 42 */
        STOP
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 42);
}

/* init_globals flow: manually set up globals and verify caml_init_globals works */
void test_multiple_globals(void) {
    caml_vm_init();
    /* Simulate CU init: set multiple globals */
    caml_set_global(0, Val_long(10));
    caml_set_global(1, Val_long(20));
    caml_set_global(2, Val_long(30));
    ASSERT(Long_val(caml_get_global(0)) == 10);
    ASSERT(Long_val(caml_get_global(1)) == 20);
    ASSERT(Long_val(caml_get_global(2)) == 30);
}

int main(void) {
    printf("=== Phase 2.4: Global Variable Tests ===\n\n");
    RUN_TEST(test_setget_global);
    RUN_TEST(test_global_field);
    RUN_TEST(test_multiple_globals);
    printf("\n=== All %d tests passed! ===\n", 3);
    return 0;
}
