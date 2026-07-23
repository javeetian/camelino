/**
 * @file test/test_phase5.c
 * @brief Phase 5 P1 指令测试
 */
#include <stdio.h>
#include <assert.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/opcodes.h"

/* ffi.c — linked separately in CMake build; for standalone syntax check */
extern void caml_init_primitives(void);

#define RUN_TEST(name) do{printf("  %-35s",#name);name();printf("OK\n");}while(0)
#define ASSERT(e) assert(e)

static void run(uint8_t* bc, size_t len) {
    caml_vm_init(); caml_load_bytecode_buf(bc, len, NULL, 0); caml_interpret();
}

/* Test 1: BEQ: 3==3 → jump */
void test_beq_true(void) {
    /* CONST3, PUSH, CONST3, BEQ(offset10→ skip CONST0+STOP), CONST0, STOP, CONST1, STOP */
    uint8_t bc[] = {
        CONST3, PUSH, CONST3,
        BEQ, 10,0,0,0,         /* if equal → jump 6 bytes */
        CONST0, STOP,          /* not taken */
        CONST1, STOP           /* taken: acc=1 */
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);
}

/* Test 2: BNEQ: 3!=5 → jump */
void test_bneq_true(void) {
    uint8_t bc[] = {
        CONSTINT, 5,0,0,0, PUSH, CONST3,
        BNEQ, 14,0,0,0, CONST0, STOP, CONST1, STOP
    };
    run(bc, sizeof(bc));
    ASSERT(Long_val(caml_get_acc()) == 1);
}

/* Test 3: BOOLNOT */
void test_boolnot(void) {
    uint8_t bc[] = { CONST0, BOOLNOT, STOP };
    run(bc, sizeof(bc));
    ASSERT(caml_get_acc() == Val_true);  /* not false = true */
}

/* Test 4: ISINT */
void test_isint(void) {
    uint8_t bc[] = { CONSTINT, 42,0,0,0, ISINT, STOP };
    run(bc, sizeof(bc));
    ASSERT(caml_get_acc() == Val_true);
}

/* 5.3: create_string(3) + string_length → 3 */
void test_string_create_len(void) {
    /* caml_create_string = index 10. Call C_CALL1(10, Val_long(3)) */
    uint8_t bc[] = {
        CONSTINT, 3,0,0,0, PUSH,
        C_CALL1, 10,0,0,0,      /* create_string(3) → string block */
        PUSH,                     /* save result */
        C_CALL1, 8,0,0,0,        /* string_length(result) */
        STOP
    };
    caml_vm_init(); caml_init_primitives();
    caml_load_bytecode_buf(bc, sizeof(bc), NULL, 0); caml_interpret();
    ASSERT(Long_val(caml_get_acc()) == 3);
}

/* 5.3: hash_variant(42) returns a hash */
void test_hash(void) {
    /* caml_hash_variant = index 26 */
    uint8_t bc[] = {
        CONSTINT, 42,0,0,0, PUSH,
        C_CALL1, 26,0,0,0, STOP
    };
    caml_vm_init(); caml_init_primitives();
    caml_load_bytecode_buf(bc, sizeof(bc), NULL, 0); caml_interpret();
    ASSERT(caml_get_acc() != Val_long(0)); /* hash should be non-zero */
}

int main(void) {
    printf("=== Phase 5: P1 Instruction Tests ===\n\n");
    RUN_TEST(test_beq_true);
    RUN_TEST(test_bneq_true);
    RUN_TEST(test_boolnot);
    RUN_TEST(test_isint);
    RUN_TEST(test_string_create_len);
    RUN_TEST(test_hash);
    printf("\n=== All %d tests passed! ===\n", 6);
    return 0;
}
