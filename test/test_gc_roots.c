/**
 * @file test/test_gc_roots.c
 * @brief Phase 4.2 CAMLparam/CAMLlocal/CAMLreturn GC 根保护测试
 */
#include <stdio.h>
#include <assert.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/gc_roots.h"

#define RUN_TEST(name) do{printf("  %-35s",#name);name();printf("OK\n");}while(0)
#define ASSERT(e) assert(e)

extern value caml_get_acc(void);

/* 模拟 C primitive: 用 CAMLparam 保护参数，内部分配可能触发 GC */
static value test_prim_alloc(value arg) {
    CAMLparam1(arg);
    /* 分配一些垃圾触发 GC */
    for (int i = 0; i < 30; i++) {
        caml_alloc(5, 0);
    }
    /* arg 应该未被 GC 回收 — 通过 CAMLparam 保护 */
    CAMLreturn(arg);
}

static value test_prim_local(value arg) {
    CAMLparam1(arg);
    CAMLlocal1(result);
    result = caml_alloc(1, 0);
    Field(result, 0) = Val_long(Long_val(arg) + 10);
    for (int i = 0; i < 30; i++) caml_alloc(5, 0);
    CAMLreturn(result);
}

void test_gc_roots_simple(void) {
    caml_memory_init();
    caml_gc_roots_init();

    value v = Val_long(42);
    value r = test_prim_alloc(v);
    ASSERT(r == v);  /* should survive GC */
}

void test_gc_roots_alloc(void) {
    caml_memory_init();
    caml_gc_roots_init();

    value v = Val_long(5);
    value r = test_prim_local(v);
    /* result should be Field(r,0) = 5 + 10 = 15 */
    ASSERT(Long_val(Field(r, 0)) == 15);
}

int main(void) {
    printf("=== Phase 4.2: GC Roots Tests ===\n\n");
    RUN_TEST(test_gc_roots_simple);
    RUN_TEST(test_gc_roots_alloc);
    printf("\n=== All %d tests passed! ===\n", 2);
    return 0;
}
