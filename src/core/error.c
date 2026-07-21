/**
 * @file core/error.c
 * @brief 错误处理实现 — fatal 错误输出 + 断言
 */

#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void caml_fatal_error(const char* msg) {
    fprintf(stderr, "CAMELINO FATAL: %s\n", msg);
#ifdef CAMELINO_PLATFORM_ARDUINO
    /* Arduino: 串口输出后死循环 */
    while (1) {}
#else
    exit(1);
#endif
}

void caml_fatal_error_code(caml_err_t code, const char* detail) {
    fprintf(stderr, "CAMELINO FATAL [%d]: %s\n", (int)code, detail ? detail : "(no detail)");
#ifdef CAMELINO_PLATFORM_ARDUINO
    while (1) {}
#else
    exit(1);
#endif
}
