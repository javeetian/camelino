/**
 * @file core/camelino_config.h
 * @brief 平台配置聚合 — 各 platform/<name>/platform_config.h 的统一定义
 *
 * 包含路径由构建系统配置（-I 指向目标平台目录）。
 */

#ifndef CAMELINO_CONFIG_H
#define CAMELINO_CONFIG_H

#include "platform_config.h"   /* 由各平台提供 */

/* ---- 编译期静态断言（C99 兼容 typedef 技法）---- */

#define CAMELINO_STATIC_ASSERT3(cond, line) \
    typedef char static_assertion_at_line_##line[(cond) ? 1 : -1]
#define CAMELINO_STATIC_ASSERT2(cond, line) CAMELINO_STATIC_ASSERT3(cond, line)
#define CAMELINO_STATIC_ASSERT(cond)        CAMELINO_STATIC_ASSERT2(cond, __LINE__)

/* 字长一致性：sizeof(void*) 必须与声明的 CAMELINO_WORD_SIZE 匹配 */
#if (CAMELINO_WORD_SIZE == 32)
#  define CAMELINO_VALUE_BITS 31
CAMELINO_STATIC_ASSERT(sizeof(void*) == 4);
#elif (CAMELINO_WORD_SIZE == 64)
#  define CAMELINO_VALUE_BITS 63
CAMELINO_STATIC_ASSERT(sizeof(void*) == 8);
#else
#  error "CAMELINO_WORD_SIZE must be 32 or 64"
#endif

/* 字节序标志必须定义 */
#ifndef CAMELINO_BIG_ENDIAN
#  error "CAMELINO_BIG_ENDIAN must be defined (0=little, 1=big)"
#endif

/* FPU 标志必须定义 */
#ifndef CAMELINO_HAS_FPU
#  error "CAMELINO_HAS_FPU must be defined (0=soft, 1=hard)"
#endif

/* 内存配置默认值（可被 platform_config.h 覆盖） */
#ifndef HEAP_SIZE
#  define HEAP_SIZE      (192 * 1024)
#endif
#ifndef STACK_SIZE
#  define STACK_SIZE     8192
#endif
#ifndef MAX_GLOBALS
#  define MAX_GLOBALS    4096
#endif

/* ---- 调试开关（§0.3.4）---- */

#ifndef CAMELINO_DEBUG_TRACE
#  define CAMELINO_DEBUG_TRACE 0   /* 1=启用指令级 trace，0=关闭 */
#endif

#ifndef CAMELINO_DEBUG_GC
#  define CAMELINO_DEBUG_GC    0   /* 1=启用 GC 统计输出，0=关闭 */
#endif

#endif /* CAMELINO_CONFIG_H */
