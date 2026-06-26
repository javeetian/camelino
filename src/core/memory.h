// camelino_memory.h
#ifndef CAMELINO_MEMORY_H
#define CAMELINO_MEMORY_H

#include "camelino_value.h"

// 内存区域配置（针对 RP2350 的 520KB SRAM）
typedef struct {
    void* heap_start;      // 堆起始地址
    void* heap_end;        // 堆结束地址
    size_t stack_size;     // C 栈预留
    size_t gc_threshold;   // GC 触发阈值
} caml_memory_config_t;

// 默认配置：堆 256KB，栈 64KB，GC 阈值 200KB
#define CAML_DEFAULT_CONFIG { \
    .heap_start = (void*)0x20000000, \
    .heap_end = (void*)0x20040000,   \
    .stack_size = 65536,             \
    .gc_threshold = 204800           \
}

// 内存分配器 API
void caml_memory_init(const caml_memory_config_t* config);
void* caml_alloc(size_t size, int tag);
void caml_gc_mark(caml_value_t v);
void caml_gc_sweep(void);
void caml_gc_collect(void);

#endif