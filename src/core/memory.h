/**
 * @file core/memory.h
 * @brief 静态内存池 + bump 分配器（Phase 1.2，无 GC）
 *
 * Design.md §6.2 布局：
 *   heap    — 静态 uint8_t 数组，bump alloc（Phase 3 加入 GC）
 *   stack   — ZAM 值栈，从高端向低端增长
 *   globals — 全局变量区，按槽位号索引
 */

#ifndef CAMELINO_MEMORY_H
#define CAMELINO_MEMORY_H

#include "platform.h"     /* value, header_t, mlsize_t, tag_t */
#include "camelino_config.h"

/* ---- API ---- */

void  caml_memory_init(void);

/* 分配堆块（返回指向 field0 的 value，header 在返回指针之前） */
value caml_alloc(mlsize_t wosize, tag_t tag);
value caml_alloc_small(mlsize_t wosize, tag_t tag);  /* 同 caml_alloc（Phase 3 优化） */

/* 值栈 */
void  caml_stack_init(void);
value* caml_stack_pointer(void);       /* 当前 sp */
void  caml_stack_push(value v);
value caml_stack_pop(void);
int   caml_stack_check_overflow(void); /* 1 = 溢出 */

/* 全局变量 */
void  caml_globals_init(void);
value caml_global_get(mlsize_t slot);
void  caml_global_set(mlsize_t slot, value v);

/* 堆统计（调试用） */
size_t caml_heap_used(void);
size_t caml_heap_total(void);

/* GC */
void caml_gc_collect(void);

#endif
