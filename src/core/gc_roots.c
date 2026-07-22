/**
 * @file core/gc_roots.c
 * @brief GC 根保护实现 (Phase 4.2)
 */
#include "gc_roots.h"
#include <stddef.h>

struct caml__roots_block* caml_local_roots = NULL;

void caml_gc_roots_init(void) {
    caml_local_roots = NULL;
}

void caml_gc_scan_roots(void (*mark)(value)) {
    struct caml__roots_block* rb = caml_local_roots;
    while (rb) {
        int n = rb->nroots + rb->nlocals;
        for (int i = 0; i < n; i++) {
            mark(rb->roots[i]);
        }
        rb = rb->next;
    }
}
