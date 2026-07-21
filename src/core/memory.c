/**
 * @file core/memory.c
 * @brief 静态内存池 + bump 分配器实现
 *
 * 堆块布局：
 *   [header_t header] [value fields...]
 *    ↑                  ↑
 *    alloc 基址         caml_alloc 返回值（field0 指针）
 *
 *   总字节 = (wosize + 1) * sizeof(value)
 */

#include "memory.h"
#include "value.h"
#include "error.h"
#include <stddef.h>

/* ---- 静态内存池（Design.md §6.2）---- */

static uint8_t  camelino_heap[HEAP_SIZE];
static value    camelino_stack[STACK_SIZE];
static value    camelino_globals[MAX_GLOBALS];

/* ---- 堆 bump 分配器 ---- */

static uint8_t* heap_ptr = NULL;    /* 当前 bump 指针（下一个 block 的起始） */
static uint8_t* heap_end = NULL;
static size_t   heap_used_bytes = 0;

void caml_memory_init(void) {
    heap_ptr = camelino_heap;
    heap_end = camelino_heap + HEAP_SIZE;
    heap_used_bytes = 0;

    caml_stack_init();
    caml_globals_init();
}

value caml_alloc(mlsize_t wosize, tag_t tag) {
    /* 总字节 = header + wosize 个 value */
    size_t total = (wosize + 1) * sizeof(value);

    /* bump 分配 */
    uint8_t* block = heap_ptr;
    if (block + total > heap_end) {
        caml_fatal_error_code(CAMELINO_ERR_OUT_OF_MEMORY, "heap exhausted (no GC yet)");
    }
    heap_ptr += total;
    heap_used_bytes += total;

    /* 写 header 在 block 起始 */
    header_t* hdr = (header_t*)block;
    *hdr = Make_header(wosize, tag, Caml_white);

    /* 返回值 = 指向 field0 的 value */
    value v = (value)(uintptr_t)(block + sizeof(header_t));

    return v;
}

value caml_alloc_small(mlsize_t wosize, tag_t tag) {
    return caml_alloc(wosize, tag);  /* Phase 3 小对象池优化 */
}

size_t caml_heap_used(void)  { return heap_used_bytes; }
size_t caml_heap_total(void) { return HEAP_SIZE; }

/* ---- 值栈 ---- */

static value* sp = NULL;

void caml_stack_init(void) {
    sp = camelino_stack + STACK_SIZE;   /* 高端（越过末尾），首次 push 前 --sp */
}

value* caml_stack_pointer(void) {
    return sp;
}

void caml_stack_push(value v) {
    if (sp <= camelino_stack) {
        caml_fatal_error_code(CAMELINO_ERR_STACK_OVERFLOW, "stack exhausted");
    }
    *(--sp) = v;
}

value caml_stack_pop(void) {
    if (sp >= camelino_stack + STACK_SIZE) {
        caml_fatal_error_code(CAMELINO_ERR_STACK_OVERFLOW, "stack underflow");
    }
    return *(sp++);
}

int caml_stack_check_overflow(void) {
    /* 留 guard zone：底部 8 slot */
    return (sp <= camelino_stack + 8) ? 1 : 0;
}

/* ---- 全局变量 ---- */

static mlsize_t globals_count = 0;

void caml_globals_init(void) {
    globals_count = 0;
}

value caml_global_get(mlsize_t slot) {
    if (slot >= globals_count) {
        caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "global slot out of range");
    }
    return camelino_globals[slot];
}

void caml_global_set(mlsize_t slot, value v) {
    if (slot >= MAX_GLOBALS) {
        caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "global slot exceeds MAX_GLOBALS");
    }
    camelino_globals[slot] = v;
    if (slot >= globals_count) {
        globals_count = slot + 1;
    }
}
