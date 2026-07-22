/**
 * @file core/memory.c
 * @brief 静态内存池 + bump 分配器 + Mark-Sweep GC (Phase 3)
 */
#include "memory.h"
#include "value.h"
#include "error.h"
#include <stddef.h>

/* ---- 静态内存池 ---- */
static uint8_t  camelino_heap[HEAP_SIZE];
static value    camelino_stack[STACK_SIZE];
static value    camelino_globals[MAX_GLOBALS];

/* ---- 堆 bump 分配器 ---- */
static uint8_t* heap_ptr = NULL;
static uint8_t* heap_end = NULL;
static size_t   heap_used_bytes = 0;
static int      gc_running = 0;      /* 防止 GC 递归 */

/* ---- free list ---- */
typedef struct free_block {
    struct free_block* next;
    size_t             size;          /* 总字节数（header + fields） */
} free_block_t;
static free_block_t* free_list = NULL;

/* ---- GC 根枚举依赖 vm.c —— 弱桩用于独立测试 ---- */
extern value caml_get_acc(void); /* vm.c */
extern value caml_get_env(void); /* vm.c */


void caml_memory_init(void) {
    heap_ptr = camelino_heap;
    heap_end = camelino_heap + HEAP_SIZE;
    heap_used_bytes = 0;
    free_list = NULL;
    gc_running = 0;
    caml_stack_init();
    caml_globals_init();
}

/* ---- block 迭代辅助 ---- */
static uint8_t* next_block(uint8_t* p) {
    header_t* h = (header_t*)p;
    mlsize_t wosize = Wosize_hd(*h);
    return p + (wosize + 1) * sizeof(value);
}

static int is_valid_block(uint8_t* p, uint8_t* limit) {
    if (p + sizeof(header_t) > limit) return 0;
    header_t* h = (header_t*)p;
    mlsize_t wosize = Wosize_hd(*h);
    return (p + (wosize + 1) * sizeof(value) <= limit);
}

/* ---- 全局变量 ---- */
static mlsize_t globals_count = 0;

void caml_globals_init(void) { globals_count = 0; }

/* ---- GC mark ---- */
static void gc_mark_value(value v) {
    if (!Is_block(v)) return;
    uint8_t* p = (uint8_t*)(v) - sizeof(header_t);
    /* Range check */
    if (p < camelino_heap || p >= heap_ptr) return;
    header_t* h = (header_t*)p;
    if (Color_hd(*h) == Caml_black) return;
    *h = Make_header(Wosize_hd(*h), Tag_hd(*h), Caml_black);
    if (Tag_is_no_scan(Tag_hd(*h))) return;
    mlsize_t sz = Wosize_hd(*h);
    value vv = (value)(uintptr_t)(p + sizeof(header_t));
    for (mlsize_t i = 0; i < sz; i++) {
        gc_mark_value(Field(vv, i));
    }
}

static void gc_mark_roots(void) {
    /* Value stack: only live portion (sp to top) */
    value* sp = caml_stack_pointer();
    value* top = camelino_stack + STACK_SIZE;
    for (value* p = sp; p < top; p++) {
        gc_mark_value(*p);
    }
    /* Global variables: only initialized slots */
    for (mlsize_t i = 0; i < globals_count; i++) {
        gc_mark_value(camelino_globals[i]);
    }
    /* Registers */
    gc_mark_value(caml_get_acc());
    gc_mark_value(caml_get_env());
}

/* ---- GC sweep ---- */
static void gc_sweep(void) {
    free_list = NULL;
    uint8_t* p = camelino_heap;
    heap_used_bytes = 0;
    uint8_t* last_live = camelino_heap;

    while (is_valid_block(p, heap_ptr)) {
        header_t* h = (header_t*)p;
        mlsize_t wosize = Wosize_hd(*h);
        size_t total = (wosize + 1) * sizeof(value);

        if (Color_hd(*h) == Caml_white) {
            /* Unreachable: add to free list */
            free_block_t* fb = (free_block_t*)p;
            fb->size = total;
            fb->next = free_list;
            free_list = fb;
        } else {
            /* Live: reset color to white for next GC cycle */
            *h = Make_header(wosize, Tag_hd(*h), Caml_white);
            last_live = p + total;
            heap_used_bytes += total;
        }
        p += total;
    }
    /* Reset heap_ptr to after last live block */
    heap_ptr = (last_live > camelino_heap) ? last_live : camelino_heap;
}

/* ---- GC entry point ---- */
void caml_gc_collect(void) {
    if (gc_running) return;
    gc_running = 1;
    gc_mark_roots();
    gc_sweep();
    gc_running = 0;
}

/* ---- alloc with GC ---- */
value caml_alloc(mlsize_t wosize, tag_t tag) {
    size_t total = (wosize + 1) * sizeof(value);

    /* 1. Try free list first */
    free_block_t** prev = &free_list;
    free_block_t* fb = free_list;
    while (fb) {
        if (fb->size >= total) {
            /* Found a fit */
            if (fb->size >= total + sizeof(free_block_t) + sizeof(value)) {
                /* Split: use first part, keep remainder */
                free_block_t* remainder = (free_block_t*)((uint8_t*)fb + total);
                remainder->size = fb->size - total;
                remainder->next = fb->next;
                *prev = remainder;
            } else {
                /* Use entire block */
                *prev = fb->next;
            }
            header_t* h = (header_t*)fb;
            *h = Make_header(wosize, tag, Caml_white);
            return (value)(uintptr_t)((uint8_t*)fb + sizeof(header_t));
        }
        prev = &fb->next;
        fb = fb->next;
    }

    /* 2. Try bump allocator */
    uint8_t* block = heap_ptr;
    if (block + total <= heap_end) {
        heap_ptr += total;
        heap_used_bytes += total;
        header_t* h = (header_t*)block;
        *h = Make_header(wosize, tag, Caml_white);
        return (value)(uintptr_t)(block + sizeof(header_t));
    }

    /* 3. Trigger GC and retry */
    caml_gc_collect();

    /* Try free list again after GC */
    fb = free_list;
    while (fb) {
        if (fb->size >= total) {
            if (fb->size >= total + sizeof(free_block_t) + sizeof(value)) {
                free_list = (free_block_t*)((uint8_t*)fb + total);
                free_list->size = fb->size - total;
                free_list->next = fb->next;
            } else {
                free_list = fb->next;
            }
            header_t* h = (header_t*)fb;
            *h = Make_header(wosize, tag, Caml_white);
            return (value)(uintptr_t)((uint8_t*)fb + sizeof(header_t));
        }
        fb = fb->next;
    }

    /* 4. Bump after GC */
    block = heap_ptr;
    if (block + total <= heap_end) {
        heap_ptr += total;
        heap_used_bytes += total;
        header_t* h = (header_t*)block;
        *h = Make_header(wosize, tag, Caml_white);
        return (value)(uintptr_t)(block + sizeof(header_t));
    }

    /* 5. OOM */
    caml_fatal_error_code(CAMELINO_ERR_OUT_OF_MEMORY, "heap exhausted after GC");
    return (value)0;
}

value caml_alloc_small(mlsize_t wosize, tag_t tag) {
    return caml_alloc(wosize, tag);
}

size_t caml_heap_used(void)  { return heap_used_bytes; }
size_t caml_heap_total(void) { return HEAP_SIZE; }

/* ---- 值栈 ---- */
static value* sp = NULL;

void caml_stack_init(void) {
    sp = camelino_stack + STACK_SIZE;
}

value* caml_stack_pointer(void) { return sp; }

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
    return (sp <= camelino_stack + 8) ? 1 : 0;
}

value caml_global_get(mlsize_t slot) {
    if (slot >= globals_count) return Val_long(0);
    return camelino_globals[slot];
}

void caml_global_set(mlsize_t slot, value v) {
    if (slot >= MAX_GLOBALS)
        caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "global slot exceeds MAX_GLOBALS");
    camelino_globals[slot] = v;
    if (slot >= globals_count) globals_count = slot + 1;
}
