/**
 * @file core/value.h
 * @brief OCaml 值表示系统 — 与 OCaml 官方 runtime 对齐（Design.md §5）
 *
 * 最低位约定（与官方一致）：
 *   整数：最低位 = 1  → Is_long(x) 为真
 *   指针：最低位 = 0  → Is_block(x) 为真（malloc 返回值对齐天然低位为 0，指针无需编码）
 *
 * 内存布局（32-bit word）：
 *   value: [31................1][1]  tagged int
 *   value: [31................1][0]  heap pointer
 *
 * header（与 value 等宽）：[Wosize | Color(2) | Tag(8)]
 *   低 8 位 = tag，次低 2 位 = GC color，其余 = wosize（§5.0）
 *
 * @copyright MIT License
 */

#ifndef CAMELINO_VALUE_H
#define CAMELINO_VALUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platform.h"          /* value, header_t, tag_t, color_t, mlsize_t */

/*==============================================================================
 * Tagged integer — OCaml 官方约定（整数低位 = 1）
 *============================================================================*/

/* 标记约定：最低位 1 → 整数，0 → 堆块指针 */
#define Is_long(x)   (((value)(x) & 1) != 0)
#define Is_block(x)  (((value)(x) & 1) == 0)

/* 整数编码：tagged int = (n << 1) | 1 */
static inline value Val_long(intptr_t n) {
    return ((value)((uintptr_t)n) << 1) | 1;
}
#define Val_int(n)   Val_long(n)

static inline intptr_t Long_val(value v) {
    return ((intptr_t)v) >> 1;   /* 算术右移保留符号 */
}
#define Int_val(v)   Long_val(v)

/*==============================================================================
 * 堆块指针 — 对齐保证低位为 0，指针 = 裸地址
 *============================================================================*/

/* 块指针：value 直接 = 堆地址（无 OR），解码 = 直接当指针 */
#define Bp_val(v)    ((value*)(v))

/* header 在 block[0] 之前，即 block[-1] */
#define Hd_val(b)    (((header_t*)(value*)(b))[-1])

/* 块字段访问（宏，支持 lvalue 赋值） */
#define Field(x, i)  (((value*)(x))[i])

/*==============================================================================
 * Header 操作（Design.md §5.0）
 *
 * 布局（指针宽）：[Wosize (高位) | Color(2, bits 8-9) | Tag(8, bits 0-7)]
 *
 * 例（32-bit）： [Wosize 22bit][Color 2bit][Tag 8bit]
 * 例（64-bit）： [Wosize 54bit][Color 2bit][Tag 8bit]
 *============================================================================*/

/* GC 颜色常量 (§6.3) */
#define Caml_white  0
#define Caml_gray   1
#define Caml_black  2
#define Caml_blue   3    /* 空闲块（free list），不参与 sweep */

#define Make_header(wosize, tag, color) \
    (((header_t)(wosize) << 10) | ((header_t)(color) << 8) | (header_t)(tag))

#define Wosize_hd(hd)   ((mlsize_t)((hd) >> 10))
#define Tag_hd(hd)      ((tag_t)((hd) & 0xFF))
#define Color_hd(hd)    ((color_t)(((hd) >> 8) & 0x3))

/*==============================================================================
 * 常用 OCaml 值常量
 *============================================================================*/

#define Val_unit     Val_long(0)
#define Val_true     Val_long(1)
#define Val_false    Val_long(0)
#define Val_empty    Val_long(0)

/*==============================================================================
 * 堆块标签常量（与 OCaml 4.14 对齐，Design.md §5.1）
 *============================================================================*/

#define Closure_tag       247
#define Lazy_tag          246
#define Object_tag        248
#define Infix_tag         249
#define Forward_tag       250
#define Abstract_tag      251      /* 也叫 No_scan_tag — GC 不扫描字段 */
#define No_scan_tag       251
#define String_tag        252
#define Double_tag        253
#define Double_array_tag  254
#define Custom_tag        255

#define Tag_is_no_scan(t)  ((t) >= No_scan_tag)

/*==============================================================================
 * 类型转换辅助
 *============================================================================*/

static inline value Val_bool(bool b) {
    return b ? Val_true : Val_false;
}
static inline bool Bool_val(value v) {
    return Long_val(v) != 0;
}

static inline value Val_char(char c) {
    return Val_long((intptr_t)c);
}

/*==============================================================================
 * 分配器 API 声明（memory.c 实现）
 *============================================================================*/

value caml_alloc(mlsize_t wosize, tag_t tag);
value caml_alloc_small(mlsize_t wosize, tag_t tag);
value caml_alloc_string(mlsize_t len);       /* len 为字节数，非 word 数 */

/*==============================================================================
 * 调试
 *============================================================================*/

void caml_debug_print_value(value v, char* out, size_t out_size);

/*==============================================================================
 * 过渡期兼容别名（整合完成后删除）
 *
 * caml_value_t / caml_header_t 定义在 platform.h 中，此处不重复 typedef。
 *============================================================================*/

/* 旧函数别名 */

#define CAML_TAG_INT       1     /* 整数低位=1（与 Is_long 一致） */
#define CAML_TAG_PTR       0     /* 指针低位=0（与 Is_block 一致） */
#define CAML_TAG_MASK      1

#define Caml_val_int(x)    Val_long(x)
#define Caml_int_val(v)    Long_val(v)
#define Caml_is_int(v)     Is_long(v)
#define Caml_is_ptr(v)     Is_block(v)
#define Caml_field(v,i)    Field(v,i)

#define CAML_UNIT          Val_unit
#define CAML_TRUE          Val_true
#define CAML_FALSE         Val_false

#define Caml_val_bool(b)   Val_bool(b)
#define Caml_bool_val(v)   Bool_val(v)
#define Caml_val_char(c)   Val_char(c)

/* Caml_val_ptr/caml_ptr_val — 过渡期兼容（OCaml 指针无需编码，直接转 value） */
static inline caml_value_t Caml_val_ptr(void* p) {
    return (caml_value_t)(uintptr_t)p;
}
static inline void* caml_ptr_val(caml_value_t v) {
    return (void*)(uintptr_t)v;
}
#define Caml_is_ptr(v)     Is_block(v)

#endif /* CAMELINO_VALUE_H */
