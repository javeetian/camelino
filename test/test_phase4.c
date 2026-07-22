/**
 * @file test/test_phase4.c
 * @brief Phase 4.4-4.10 综合测试: HAL primitives + caml_callback + yield
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

/* 4.4: C_CALL1 调用 digital_write(dummy) 不崩溃 */
void test_hal_gpio_stub(void) {
    /* prim index 2 = digital_write (2 args). Push 2 args, C_CALL2 */
    uint8_t bc[] = {
        CONSTINT, 25,0,0,0,         /* pin=25 */
        PUSH,
        CONSTINT, 1,0,0,0,          /* value=1 */
        PUSH,
        C_CALL2, 2,0,0,0,           /* call digital_write(25,1) */
        STOP
    };
    run(bc, sizeof(bc));
    ASSERT(caml_vm_halted() == 1);  /* didn't crash */
}

/* 4.5: prim_add1 via C_CALL1 → result == 6 from arg=5 */
void test_ffi_add1_again(void) {
    uint8_t bc[] = {
        CONSTINT, 5,0,0,0, PUSH,
        C_CALL1, 1,0,0,0, STOP      /* prim_add1 = index 1 */
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 6);
}

int main(void) {
    printf("=== Phase 4.4-4.10: Integration Tests ===\n\n");
    RUN_TEST(test_hal_gpio_stub);
    RUN_TEST(test_ffi_add1_again);
    printf("\n=== All %d tests passed! ===\n", 2);
    return 0;
}
