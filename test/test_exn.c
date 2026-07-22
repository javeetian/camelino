/**
 * @file test/test_exn.c
 * @brief Phase 2.3 异常处理测试
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

/* Test 1: RAISE jumps to handler. Handler sees exception value in acc. */
void test_raise_basic(void) {
    /* PUSHTRAP(offset=5): handler at pc+4+5=9 = STOP
       CONST3, RAISE → jump handler. Handler: STOP with acc=3 */
    uint8_t bc[] = {
        PUSHTRAP, 5,0,0,0,          /* 0-4 */
        CONSTINT, 3,0,0,0,           /* 5-9 */
        RAISE,                        /* 10: jump to handler */
        STOP,                         /* 11: handler (pc+4+5=11? No, 0+4+5=9=CONSTINT byte) */

        /* Hmm, offset calculation: pc at offset 0 after opcode, +4 = offset 4.
           offset 4 + 5 = 9. Byte 9 = 3 (part of CONSTINT operand). That's wrong!

           pc+offset: pc after reading PUSHTRAP opcode points to the 4-byte operand.
           pc = &bc[1]. offset 4 bytes (the operand itself) + 5 = 9. So pc+4+5 = &bc[9]
           = byte 9 = 3. That's data not code!

           Fix: offset should be larger to skip past RAISE to the handler.
           Let me use a cleaner layout. */
    };
    /* Discard and write clean version below */
    (void)bc;
}

/* Test 1: RAISE jumps to handler, acc preserved */
void test_raise_basic_clean(void) {
    /* PUSHTRAP(offset=12) → handler at &bc[1]+4+12 = &bc[17]
       CONSTINT(3) → RAISE → handler(STOP).
       RAISE should keep acc=3, handler just halts. */
    uint8_t bc[] = {
        PUSHTRAP, 12,0,0,0,        /* 0-4: handler = pc+4+12 = 17 */
        CONSTINT, 3,0,0,0,         /* 5-9: acc=3 */
        RAISE,                      /* 10: jump to handler */
        CONSTINT, 0,0,0,0,         /* 11-15: dead code (padding) */
        STOP,                       /* 16 */
        STOP                        /* 17: handler */
    };
    run(bc, sizeof(bc));
    /* After RAISE, acc should be the exception value = Val_long(3) */
    /* Handler just STOPs. VM halts with acc=3. */
    ASSERT(caml_vm_halted() == 1);
    ASSERT(Long_val(caml_get_acc()) == 3);
}

/* Test 2: uncaught RAISE → VM halts */
void test_raise_uncaught(void) {
    uint8_t bc[] = { CONSTINT, 99,0,0,0, RAISE, STOP };
    run(bc, sizeof(bc));
    ASSERT(caml_vm_halted() == 1);
}

int main(void) {
    printf("=== Phase 2.3: Exception Tests ===\n\n");
    RUN_TEST(test_raise_basic_clean);
    RUN_TEST(test_raise_uncaught);
    printf("\n=== All %d tests passed! ===\n", 2);
    return 0;
}
