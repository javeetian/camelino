/**
 * @file ffi/ffi.c
 * @brief C primitive 注册表 (Phase 4)
 */
#include "ffi.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    const char* name;
    void*       func;
    int         arity;
} prim_entry_t;

static prim_entry_t prim_table[CAML_MAX_PRIMITIVES];
static int prim_count = 0;

void caml_init_primitives(void) {
    prim_count = 0;
    memset(prim_table, 0, sizeof(prim_table));
    caml_register_builtin_primitives();
}

int caml_register_primitive(const char* name, void* func, int arity) {
    if (prim_count >= CAML_MAX_PRIMITIVES) return -1;
    prim_table[prim_count].name = name;
    prim_table[prim_count].func = func;
    prim_table[prim_count].arity = arity;
    prim_count++;
    return prim_count - 1;  /* return index */
}

void* caml_lookup_primitive(const char* name) {
    for (int i = 0; i < prim_count; i++) {
        if (prim_table[i].name && strcmp(prim_table[i].name, name) == 0)
            return prim_table[i].func;
    }
    return NULL;
}

void* caml_lookup_primitive_by_index(int index, int* arity) {
    if (index < 0 || index >= prim_count) return NULL;
    if (arity) *arity = prim_table[index].arity;
    return prim_table[index].func;
}

/* ---- 内置 primitives (Phase 4) ---- */

/* 测试 */
static value prim_identity(value x) { return x; }
static value prim_add1(value x) { return Val_long(Long_val(x) + 1); }

/* 4.4 Bindings: HAL GPIO/UART/Time 桩（host 端） */
static value prim_digital_write(value p, value v) {
    printf("[HAL] digital_write pin=%ld val=%ld\n", Long_val(p), Long_val(v));
    return Val_unit;
}
static value prim_digital_read(value p) {
    printf("[HAL] digital_read pin=%ld\n", Long_val(p));
    return Val_false;
}
static value prim_delay_ms(value ms) {
    printf("[HAL] delay_ms %ld\n", Long_val(ms));
    return Val_unit;
}
static value prim_millis(value u) { (void)u; return Val_long(0); }

/* 4.10 内部: 格式化桩 */
static value prim_format_int(value fmt, value n) { (void)fmt; (void)n; return Val_long(0); }
static value prim_int_of_string(value s) { (void)s; return Val_long(0); }

void caml_register_builtin_primitives(void) {
    caml_register_primitive("caml_identity", prim_identity, 1);
    caml_register_primitive("caml_add1", prim_add1, 1);

    /* 4.4 GPIO */
    caml_register_primitive("caml_camellino_digital_write", prim_digital_write, 2);
    caml_register_primitive("caml_camellino_digital_read",  prim_digital_read, 1);
    caml_register_primitive("caml_camellino_pin_mode",      prim_digital_write, 2); /* stub */
    /* 4.4 Time */
    caml_register_primitive("caml_camellino_delay_ms", prim_delay_ms, 1);
    caml_register_primitive("caml_camellino_millis",   prim_millis, 0);
    /* 4.4 UART */
    caml_register_primitive("caml_camellino_serial_write", prim_digital_write, 2); /* stub */
    /* 4.10 Format */
    caml_register_primitive("caml_format_int",      prim_format_int, 2);
    caml_register_primitive("caml_int_of_string",    prim_int_of_string, 1);
    caml_register_primitive("caml_format_float",     prim_format_int, 2);
    caml_register_primitive("caml_float_of_string",  prim_int_of_string, 1);
}
