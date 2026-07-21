/**
 * @file core/platform.h
 * @brief 平台类型定义 — OCaml runtime 基础类型（§5）
 *
 * 本文件定义与 OCaml 官方 runtime 对齐的类型。
 * 整合时替换 value.h 中的 Caml_* 前缀旧类型。
 */

#ifndef CAMELINO_CORE_PLATFORM_H
#define CAMELINO_CORE_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

/* ---- OCaml value 核心类型 (§5) ---- */

typedef intptr_t  value;       /* 与 sizeof(void*) 等宽，低位 1=整数 0=指针 */
typedef value     header_t;    /* 堆块头部，与 value 等宽 (§5.0) */

/* ---- 辅助类型 ---- */

#if CAMELINO_WORD_SIZE == 32
typedef uint32_t  mlsize_t;    /* word 计数，32 位 target 够用 */
#else
typedef size_t    mlsize_t;    /* 64 位 target 使用 size_t */
#endif

typedef uint8_t   tag_t;       /* 块标签 0-255 (§5.1) */
typedef uint8_t   color_t;     /* GC 颜色 0-3 (§5.0、§6.3) */

/* ---- 与旧 value.h 的兼容别名（整合期过渡，Phase 1 完成后删除） ---- */

typedef value     caml_value_t;
typedef header_t  caml_header_t;

#endif /* CAMELINO_CORE_PLATFORM_H */
