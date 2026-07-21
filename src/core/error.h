/**
 * @file core/error.h
 * @brief 错误处理基础设施 — 统一 fatal 错误输出 + 断言宏
 *
 * 在设备端，fatal 错误通过 UART 输出后停机；
 * 在 host 模拟器上，输出到 stderr + exit(1)。
 */

#ifndef CAMELINO_CORE_ERROR_H
#define CAMELINO_CORE_ERROR_H

#include <stdint.h>

/* ---- 错误码 ---- */

typedef enum {
    CAMELINO_OK                    = 0,
    CAMELINO_ERR_STACK_OVERFLOW    = 1,
    CAMELINO_ERR_OUT_OF_MEMORY     = 2,
    CAMELINO_ERR_BYTECODE_INVALID  = 3,
    CAMELINO_ERR_UNCAUGHT_EXN      = 4,
    CAMELINO_ERR_INTERNAL          = 5,
    CAMELINO_ERR_PRIMITIVE_MISSING = 6
} caml_err_t;

/* ---- fatal 错误（不返回） ---- */

void caml_fatal_error(const char* msg);
void caml_fatal_error_code(caml_err_t code, const char* detail);

/* ---- 断言宏 ---- */

#ifdef NDEBUG
#  define CAMELINO_ASSERT(cond)  ((void)0)
#else
#  define CAMELINO_ASSERT(cond)                                         \
     do {                                                               \
         if (!(cond)) {                                                 \
             caml_fatal_error("ASSERT failed: " #cond);                 \
         }                                                              \
     } while (0)
#endif

#endif /* CAMELINO_CORE_ERROR_H */
