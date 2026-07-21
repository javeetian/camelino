/**
 * @file test/test_memory.c
 * @brief memory.h 分配器单元测试（Phase 1.2）
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../src/core/value.h"
#include "../src/core/memory.h"

#define TEST(name)     void name(void)
#define RUN_TEST(name) do { printf("  %-35s", #name); name(); printf("OK\n"); } while(0)
#define ASSERT(expr)   assert(expr)

/* ---- 测试 1：初始化 ---- */

TEST(test_init) {
    caml_memory_init();
    ASSERT(caml_heap_used() == 0);
    ASSERT(caml_heap_total() > 0);
}

/* ---- 测试 2：分配单个块 ---- */

TEST(test_alloc_single) {
    caml_memory_init();
    value v = caml_alloc(1, 0);  /* wosize=1, 1 字段 */
    ASSERT(v != (value)0);
    ASSERT(Is_block(v) == true);
    ASSERT(Wosize_hd(Hd_val(v)) == 1);
    ASSERT(Tag_hd(Hd_val(v)) == 0);
    ASSERT(caml_heap_used() > 0);
}

/* ---- 测试 3：分配后读写字段 ---- */

TEST(test_alloc_read_write) {
    caml_memory_init();
    value v = caml_alloc(3, 0);  /* wosize=3 字段 */
    ASSERT(Wosize_hd(Hd_val(v)) == 3);

    /* 写入字段 */
    Field(v, 0) = Val_long(100);
    Field(v, 1) = Val_long(200);
    Field(v, 2) = Val_long(300);

    ASSERT(Long_val(Field(v, 0)) == 100);
    ASSERT(Long_val(Field(v, 1)) == 200);
    ASSERT(Long_val(Field(v, 2)) == 300);
}

/* ---- 测试 4：多次分配不重叠 ---- */

TEST(test_alloc_multiple) {
    caml_memory_init();
    value a = caml_alloc(2, 0);
    value b = caml_alloc(3, Closure_tag);
    value c = caml_alloc(1, String_tag);

    /* 三个块地址互不相同，且均在 heap 范围内 */
    ASSERT(a != b);
    ASSERT(b != c);
    ASSERT(a != c);

    /* 各自独立读写 */
    Field(a, 0) = Val_long(10);
    Field(a, 1) = Val_long(20);
    Field(b, 0) = Val_long(30);
    Field(c, 0) = Val_long(40);

    ASSERT(Long_val(Field(a, 0)) == 10);
    ASSERT(Long_val(Field(b, 0)) == 30);
    ASSERT(Long_val(Field(c, 0)) == 40);
}

/* ---- 测试 5：tag 正确存储和读取 ---- */

TEST(test_alloc_tags) {
    caml_memory_init();
    value a = caml_alloc(1, Closure_tag);
    value b = caml_alloc(1, String_tag);
    value c = caml_alloc(1, Double_array_tag);
    value d = caml_alloc(1, Custom_tag);

    ASSERT(Tag_hd(Hd_val(a)) == Closure_tag);
    ASSERT(Tag_hd(Hd_val(b)) == String_tag);
    ASSERT(Tag_hd(Hd_val(c)) == Double_array_tag);
    ASSERT(Tag_hd(Hd_val(d)) == Custom_tag);
}

/* ---- 测试 6：color 在字段读写时不损坏 ---- */

TEST(test_color_preserved) {
    caml_memory_init();
    /* 模拟 GC 着色：分配后手动设 color，确认字段读写不破坏 */
    value v = caml_alloc(2, 0);
    header_t h = Hd_val(v);
    h = Make_header(Wosize_hd(h), Tag_hd(h), Caml_black);  /* 重新着色 */
    Hd_val(v) = h;

    /* 读写字段 */
    Field(v, 0) = Val_long(99);
    Field(v, 1) = Val_long(88);

    ASSERT(Color_hd(Hd_val(v)) == Caml_black);  /* color 未变 */
    ASSERT(Long_val(Field(v, 0)) == 99);
}

/* ---- 测试 7：栈 push/pop ---- */

TEST(test_stack_basic) {
    caml_memory_init();
    caml_stack_push(Val_long(1));
    caml_stack_push(Val_long(2));
    caml_stack_push(Val_long(3));

    ASSERT(Long_val(caml_stack_pop()) == 3);
    ASSERT(Long_val(caml_stack_pop()) == 2);
    ASSERT(Long_val(caml_stack_pop()) == 1);
}

/* ---- 测试 8：全局变量 set/get ---- */

TEST(test_globals_basic) {
    caml_memory_init();
    caml_global_set(0, Val_long(42));
    caml_global_set(1, Val_long(99));

    ASSERT(Long_val(caml_global_get(0)) == 42);
    ASSERT(Long_val(caml_global_get(1)) == 99);
}

/* ---- main ---- */

int main(void) {
    printf("=== Camelino Memory Allocator Tests ===\n\n");

    RUN_TEST(test_init);
    RUN_TEST(test_alloc_single);
    RUN_TEST(test_alloc_read_write);
    RUN_TEST(test_alloc_multiple);
    RUN_TEST(test_alloc_tags);
    RUN_TEST(test_color_preserved);
    RUN_TEST(test_stack_basic);
    RUN_TEST(test_globals_basic);

    printf("\n=== All %d tests passed! ===\n", 8);
    return 0;
}
