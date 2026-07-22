/**
 * @file core/gc_roots.h
 * @brief GC 根保护宏 — CAMLparam/CAMLlocal/CAMLreturn (Phase 4.2)
 *
 * 用法（在 C primitive 中）:
 *   CAMLprim value my_prim(value arg) {
 *     CAMLparam1(arg);                 // 声明 arg 为 GC 根
 *     CAMLlocal1(result);
 *     result = caml_alloc(1, 0);       // 可能触发 GC
 *     Field(result, 0) = arg;
 *     CAMLreturn(result);              // 注销根并返回
 *   }
 *
 * 实现: 函数局部帧链表。GC mark 时遍历此链表。
 */
#ifndef CAMELINO_GC_ROOTS_H
#define CAMELINO_GC_ROOTS_H

#include "value.h"

/* ---- 根注册帧 ---- */
struct caml__roots_block {
    struct caml__roots_block* next;
    int    nroots;               /* 参数个数 */
    int    nlocals;              /* 局部变量个数 */
    value  roots[16];            /* 参数 + 局部变量，最多 16 个 */
};

/* 全局根链表头 */
extern struct caml__roots_block* caml_local_roots;

/* 初始化 */
void caml_gc_roots_init(void);

/* GC 扫描所有注册的根 */
void caml_gc_scan_roots(void (*mark)(value));

/* ---- 使用宏 ---- */

#define CAMLparam0() \
    struct caml__roots_block caml__roots_ctx = { caml_local_roots, 0, 0, {Val_unit} }; \
    caml_local_roots = &caml__roots_ctx

#define CAMLparam1(x) \
    struct caml__roots_block caml__roots_ctx = { caml_local_roots, 1, 0, {Val_unit} }; \
    caml_local_roots = &caml__roots_ctx; \
    caml__roots_ctx.roots[0] = (x)

#define CAMLparam2(x,y) \
    struct caml__roots_block caml__roots_ctx = { caml_local_roots, 2, 0, {Val_unit} }; \
    caml_local_roots = &caml__roots_ctx; \
    caml__roots_ctx.roots[0] = (x); caml__roots_ctx.roots[1] = (y)

#define CAMLparam3(x,y,z) \
    struct caml__roots_block caml__roots_ctx = { caml_local_roots, 3, 0, {Val_unit} }; \
    caml_local_roots = &caml__roots_ctx; \
    caml__roots_ctx.roots[0]=(x); caml__roots_ctx.roots[1]=(y); caml__roots_ctx.roots[2]=(z)

#define CAMLlocal1(x) \
    value x = Val_unit; \
    caml__roots_ctx.roots[caml__roots_ctx.nroots] = Val_unit; \
    caml__roots_ctx.nlocals = 1

#define CAMLlocal2(x,y) \
    value x = Val_unit, y = Val_unit; \
    caml__roots_ctx.roots[caml__roots_ctx.nroots] = Val_unit; \
    caml__roots_ctx.roots[caml__roots_ctx.nroots+1] = Val_unit; \
    caml__roots_ctx.nlocals = 2

#define CAMLreturn(result) \
    do { caml_local_roots = caml__roots_ctx.next; return (result); } while(0)

#define CAMLreturn0 \
    do { caml_local_roots = caml__roots_ctx.next; return Val_unit; } while(0)

#endif
