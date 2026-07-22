/**
 * @file ffi/ffi.c
 * @brief C primitive 注册表 (Phase 4)
 */
#include "ffi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

/* 5.3 String primitives (tag 252, length in padding byte) */
static mlsize_t string_length(value s) {
    header_t h = Hd_val(s);
    mlsize_t wosize = Wosize_hd(h);
    uint8_t* bytes = (uint8_t*)s;
    uint8_t pad = bytes[wosize * sizeof(value) - 1];
    return wosize * sizeof(value) - pad;
}

static value prim_string_length(value s) {
    if (!Is_block(s) || Tag_hd(Hd_val(s)) != String_tag) return Val_long(0);
    return Val_long((intptr_t)string_length(s));
}

static value prim_string_get(value s, value idx) {
    if (!Is_block(s)) return Val_long(0);
    mlsize_t i = (mlsize_t)Long_val(idx);
    if (i >= string_length(s)) return Val_long(0);
    return Val_long((intptr_t)((uint8_t*)s)[i]);
}

static value prim_create_string(value len) {
    mlsize_t n = (mlsize_t)Long_val(len);
    if (n == 0) {
        value b = caml_alloc(1, String_tag);
        uint8_t* d = (uint8_t*)b;
        d[0] = (uint8_t)sizeof(value); /* padding = word size */
        return b;
    }
    mlsize_t words = (n + sizeof(value)) / sizeof(value);
    value b = caml_alloc(words, String_tag);
    uint8_t* d = (uint8_t*)b;
    size_t pad = words * sizeof(value) - n;
    d[n] = (uint8_t)pad;
    return b;
}

static value prim_blit_string(value src, value src_ofs, value dst, value dst_ofs, value len) {
    mlsize_t slen = (mlsize_t)Long_val(src_ofs);
    mlsize_t dlen = (mlsize_t)Long_val(dst_ofs);
    mlsize_t n    = (mlsize_t)Long_val(len);
    uint8_t* s = (uint8_t*)src + slen;
    uint8_t* d = (uint8_t*)dst + dlen;
    for (mlsize_t i = 0; i < n; i++) d[i] = s[i];
    return Val_unit;
}
/* Bytes = String (tag 252), same primitives */
#define prim_bytes_length  prim_string_length
#define prim_bytes_get     prim_string_get
#define prim_create_bytes  prim_create_string
#define prim_blit_bytes    prim_blit_string

/* 5.6 通道 I/O */
static value prim_ml_output_char(value ch, value c) {
    (void)ch; putchar((int)Long_val(c)); return Val_unit;
}
static value prim_ml_input_char(value ch) {
    (void)ch; int c = getchar(); return Val_long((intptr_t)(c >= 0 ? c : -1));
}
static value prim_ml_output(value ch, value buf, value ofs, value len) {
    (void)ch; mlsize_t o = (mlsize_t)Long_val(ofs), n = (mlsize_t)Long_val(len);
    uint8_t* d = (uint8_t*)buf + o;
    for (mlsize_t i = 0; i < n; i++) putchar(d[i]); return Val_unit;
}
static value prim_ml_flush(value ch) { (void)ch; fflush(stdout); return Val_unit; }
static value prim_ml_open_descriptor_out(value fd) {
    (void)fd; return Val_long(1); /* channel 1 = stdout */
}
static value prim_ml_open_descriptor_in(value fd) {
    (void)fd; return Val_long(0); /* channel 0 = stdin */
}

/* 5.3 Random */
static unsigned _random_seed = 1;
static value prim_random_init(value seed) { _random_seed = (unsigned)Long_val(seed); return Val_unit; }
static value prim_random_int(value bound) {
    _random_seed = _random_seed * 1103515245 + 12345;
    return Val_long((intptr_t)(_random_seed % (unsigned)Long_val(bound)));
}

/* 5.3.4 Hashtbl — 多态哈希 (FNV-1a simplified) */
static intptr_t _hash_mix(intptr_t h, intptr_t v) {
    return ((h ^ v) * 16777619) & 0x7FFFFFFF;
}
static intptr_t _hash_value(value v, intptr_t seed) {
    if (Is_long(v)) return _hash_mix(seed, Long_val(v));
    if (!Is_block(v)) return seed;
    header_t hd = Hd_val(v);
    mlsize_t sz = Wosize_hd(hd);
    intptr_t h = _hash_mix(seed, Tag_hd(hd));
    if (Tag_is_no_scan(Tag_hd(hd)) || Tag_hd(hd) == String_tag) {
        /* raw bytes */
        uint8_t* d = (uint8_t*)v;
        mlsize_t n = sz * sizeof(value);
        for (mlsize_t i = 0; i < n && i < 256; i++)
            h = _hash_mix(h, d[i]);
    } else {
        for (mlsize_t i = 0; i < sz && i < 32; i++)
            h = _hash_value(Field(v, i), h);
    }
    return h;
}
static value prim_hash_variant(value v) {
    return Val_long(_hash_value(v, 0));
}
static value prim_hash_univ(value v, value seed) {
    return Val_long(_hash_value(v, Long_val(seed)));
}

/* 5.6 Lazy */
static value prim_update_dummy(value v) { return v; }
static value prim_lazy_make_forward(value v) { return v; }

/* 5.3 Printf format */
static value prim_format_int_stub(value s, value n) {
    (void)s; char buf[32]; snprintf(buf, sizeof(buf), "%ld", (long)Long_val(n));
    mlsize_t sl = (mlsize_t)strlen(buf);
    value r = prim_create_string(Val_long((intptr_t)sl));
    uint8_t* d = (uint8_t*)r;
    for (mlsize_t i = 0; i < sl; i++) d[i] = (uint8_t)buf[i];
    return r;
}

static value prim_int_of_string_stub(value s) {
    if (!Is_block(s)) return Val_long(0);
    mlsize_t sl = string_length(s);
    char buf[64] = {0};
    for (mlsize_t i = 0; i < sl && i < 63; i++) buf[i] = (char)((uint8_t*)s)[i];
    return Val_long((intptr_t)atol(buf));
}

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
    caml_register_primitive("caml_camellino_serial_write", prim_digital_write, 2);

    /* 5.3 String + Bytes */
    caml_register_primitive("caml_string_length",   prim_string_length, 1);
    caml_register_primitive("caml_string_get",      prim_string_get, 2);
    caml_register_primitive("caml_create_string",    prim_create_string, 1);
    caml_register_primitive("caml_blit_string",      prim_blit_string, 5);
    caml_register_primitive("caml_string_equal",     prim_string_length, 2); /* stub */
    caml_register_primitive("caml_bytes_length",     prim_bytes_length, 1);
    caml_register_primitive("caml_bytes_get",        prim_bytes_get, 2);
    caml_register_primitive("caml_create_bytes",      prim_create_bytes, 1);
    caml_register_primitive("caml_blit_bytes",        prim_blit_bytes, 5);

    /* 5.6 通道 I/O */
    caml_register_primitive("caml_ml_output_char",   prim_ml_output_char, 2);
    caml_register_primitive("caml_ml_input_char",    prim_ml_input_char, 1);
    caml_register_primitive("caml_ml_output",         prim_ml_output, 4);
    caml_register_primitive("caml_ml_flush",          prim_ml_flush, 1);
    caml_register_primitive("caml_ml_input",          prim_ml_input_char, 2);
    caml_register_primitive("caml_ml_open_descriptor_out", prim_ml_open_descriptor_out, 1);
    caml_register_primitive("caml_ml_open_descriptor_in",  prim_ml_open_descriptor_in, 1);

    /* 5.3 Random */
    caml_register_primitive("caml_random_init",       prim_random_init, 1);
    caml_register_primitive("caml_random_int",        prim_random_int, 1);

    /* 5.3.4 Hashtbl */
    caml_register_primitive("caml_hash_variant",      prim_hash_variant, 1);
    caml_register_primitive("caml_hash_univ",         prim_hash_univ, 2);

    /* 5.6 Lazy */
    caml_register_primitive("caml_update_dummy",      prim_update_dummy, 1);
    caml_register_primitive("caml_lazy_make_forward", prim_lazy_make_forward, 1);

    /* 4.10 Format */
    caml_register_primitive("caml_format_int",        prim_format_int_stub, 2);
    caml_register_primitive("caml_int_of_string",     prim_int_of_string_stub, 1);
    caml_register_primitive("caml_format_float",      prim_format_int_stub, 2);
    caml_register_primitive("caml_float_of_string",   prim_int_of_string_stub, 1);
}
