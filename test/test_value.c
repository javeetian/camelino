/**
 * @file test/test_value.c
 * @brief value.h 的单元测试（Design.md §5 新 tag 约定）
 *
 * 编译：cmake --build build && ctest --test-dir build
 * 运行：./build/test_value
 */

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "../src/core/value.h"

/* ---- 简单测试框架 ---- */

#define TEST(name)     void name(void)
#define RUN_TEST(name) do { \
    printf("  %-35s", #name); \
    name(); \
    printf("OK\n"); \
} while(0)
#define ASSERT(expr)   assert(expr)

/*==============================================================================
 * 测试 1：tagged integer 往返（新约定：低位 1 = 整数）
 *============================================================================*/

TEST(test_integer_roundtrip) {
    /* Val_long(0) = (0 << 1) | 1 = 1   → Long_val(1) = 1 >> 1 = 0 */
    ASSERT(Val_long(0) == 1);
    ASSERT(Long_val(1) == 0);

    /* Val_long(1) = (1 << 1) | 1 = 3   → Long_val(3) = 3 >> 1 = 1 */
    ASSERT(Val_long(1) == 3);
    ASSERT(Long_val(3) == 1);

    /* Val_long(-1) = 64-bit: 0xFFFFFFFFFFFFFFFF → 保留符号位 */
    ASSERT(Long_val(Val_long(-1)) == -1);
    ASSERT(Long_val(Val_long(42)) == 42);
    ASSERT(Long_val(Val_long(0)) == 0);
    ASSERT(Long_val(Val_long(1234567)) == 1234567);
}

/*==============================================================================
 * 测试 2：tag 判断
 *============================================================================*/

TEST(test_tag_discrimination) {
    value v_int  = Val_long(42);
    value v_zero = Val_long(0);
    value v_neg  = Val_long(-1);

    ASSERT(Is_long(v_int)  == true);
    ASSERT(Is_long(v_zero) == true);
    ASSERT(Is_long(v_neg)  == true);
    ASSERT(Is_block(v_int) == false);
}

/*==============================================================================
 * 测试 3：堆块指针（对齐保证低位为 0）与 header
 *============================================================================*/

TEST(test_pointer_and_header) {
    /* 对齐指针低位为 0 → Is_block(v) = true */
    int x = 0;
    value v = (value)(uintptr_t)(&x);

    /* 检查：对齐指针的自然特性 */
    if (((uintptr_t)&x & 1) == 0) {
        ASSERT(Is_block(v) == true);
        ASSERT(Is_long(v) == false);
    }
    /* 若指针按 word 对齐，低位即 0 — 这是 OCaml 依赖的假设 */

    /* header: Make_header(wosize, tag, color) — 低 8 位 tag，位 8-9 color，高位 wosize */
    header_t h = Make_header(3, String_tag, Caml_white);
    ASSERT(Wosize_hd(h) == 3);
    ASSERT(Tag_hd(h)  == String_tag);
    ASSERT(Color_hd(h) == Caml_white);

    /* color 着色测试 */
    header_t h2 = Make_header(5, Closure_tag, Caml_black);
    ASSERT(Tag_hd(h2)  == Closure_tag);
    ASSERT(Color_hd(h2) == Caml_black);
    ASSERT(Wosize_hd(h2) == 5);

    /* color 在 bits 8-9，不污染 tag */
    ASSERT(Tag_hd(h) == Tag_hd(Make_header(3, String_tag, Caml_white)));
}

/*==============================================================================
 * 测试 4：Hd_val / Field / Bp_val
 *============================================================================*/

TEST(test_block_access) {
    /* 模拟堆块布局：[header | field0 | field1]
     *                 ^block[0] ^block[1] ^block[2]
     * value 指针 b = &block[1]（指向第一个字段）
     * Hd_val(b) = block[0]（header 在 b[-1]） */
    value block[3];
    block[0] = Make_header(2, 0, Caml_white);  /* header */
    block[1] = Val_long(100);                   /* field0 */
    block[2] = Val_long(200);                   /* field1 */

    value b = (value)(uintptr_t)(&block[1]);    /* b 指向第一个字段 */

    ASSERT(Wosize_hd(Hd_val(b)) == 2);
    ASSERT(Tag_hd(Hd_val(b))  == 0);
    ASSERT(Long_val(Field(b, 0)) == 100);
    ASSERT(Long_val(Field(b, 1)) == 200);
    ASSERT(Is_long(Field(b, 0))  == true);
    ASSERT(Is_long(Field(b, 1))  == true);
}

/*==============================================================================
 * 测试 5：常量与 bool/char
 *============================================================================*/

TEST(test_constants_and_converters) {
    ASSERT(Is_long(Val_unit)  == true);
    ASSERT(Is_long(Val_true)  == true);
    ASSERT(Is_long(Val_false) == true);
    ASSERT(Long_val(Val_unit)  == 0);
    ASSERT(Long_val(Val_true)  == 1);

    ASSERT(Val_bool(true)  == Val_true);
    ASSERT(Val_bool(false) == Val_false);
    ASSERT(Bool_val(Val_true)  == true);
    ASSERT(Bool_val(Val_false) == false);

    /* 字符往返 */
    for (int c = -128; c < 127; c++) {
        ASSERT(Long_val(Val_char((char)c)) == c);
    }
}

/*==============================================================================
 * 测试 6：tag 常量（与 OCaml 4.14 对齐）
 *============================================================================*/

TEST(test_tag_constants) {
    ASSERT(Closure_tag      == 247);
    ASSERT(String_tag       == 252);
    ASSERT(Double_tag       == 253);
    ASSERT(Double_array_tag == 254);
    ASSERT(Custom_tag       == 255);
    ASSERT(Abstract_tag     == 251);
    ASSERT(No_scan_tag      == 251);
    ASSERT(Object_tag       == 248);
    ASSERT(Infix_tag        == 249);
    ASSERT(Forward_tag      == 250);
    ASSERT(Lazy_tag         == 246);

    /* Tag_is_no_scan */
    ASSERT(Tag_is_no_scan(Abstract_tag)     == true);
    ASSERT(Tag_is_no_scan(String_tag)       == true);
    ASSERT(Tag_is_no_scan(Double_tag)       == true);
    ASSERT(Tag_is_no_scan(Double_array_tag) == true);
    ASSERT(Tag_is_no_scan(Custom_tag)       == true);
    ASSERT(Tag_is_no_scan(Closure_tag)      == false);   /* 247 < 251 */
    ASSERT(Tag_is_no_scan(0)                == false);   /* 结构化块 */
}

/*==============================================================================
 * 测试 7：大数值测试
 *============================================================================*/

TEST(test_large_integers) {
    /* 31 位范围内（32 位 target） */
    ASSERT(Long_val(Val_long(0x3FFFFFFF)) == 0x3FFFFFFF);
    ASSERT(Long_val(Val_long(-0x40000000)) == (intptr_t)-0x40000000);

    /* 0 的 tagged 表示应为 1 */
    ASSERT(Val_long(0) == 1);
    /* Val_true 是 tagged 1 */
    ASSERT(Val_long(1) == 3);
}

/*==============================================================================
 * main
 *============================================================================*/

int main(void) {
    printf("=== Camelino Value Module Tests (OCaml-official tag convention) ===\n\n");

    RUN_TEST(test_integer_roundtrip);
    RUN_TEST(test_tag_discrimination);
    RUN_TEST(test_pointer_and_header);
    RUN_TEST(test_block_access);
    RUN_TEST(test_constants_and_converters);
    RUN_TEST(test_tag_constants);
    RUN_TEST(test_large_integers);

    printf("\n=== All %d tests passed! ===\n", 7);
    return 0;
}
