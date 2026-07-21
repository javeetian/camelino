/**
 * @file core/value.c
 * @brief value.h 中非内联函数的实现
 */

#include "value.h"
#include <string.h>
#include <stdio.h>

/*==============================================================================
 * 调试输出
 *============================================================================*/

void caml_debug_print_value(value v, char* out, size_t out_size) {
    char* p = out;
    size_t remaining = out_size;
    int n;

    if (Is_long(v)) {
        n = snprintf(p, remaining, "%ld", (long)Long_val(v));
    } else {
        n = snprintf(p, remaining, "<block:%p>", (void*)v);
    }
    p += n;

    if ((size_t)n < out_size && remaining > (size_t)n) {
        *(out + (out_size - 1)) = '\0';
    }
}
