/**
 * @file value.c
 * @brief value.h 中非内联函数的实现
 */

 #include "value.h"
 #include <string.h>
 
 /*==============================================================================
  * 内部函数实现（临时占位，实际需要 memory.c）
  *============================================================================*/
 
 // 注意：这些函数暂时返回错误值，等 memory.c 实现后会被替换
 caml_value_t caml_alloc_block_internal(unsigned int tag, size_t size) {
     (void)tag;
     (void)size;
     // TODO: 等待 memory.c 实现
     return CAML_UNIT;
 }
 
 caml_value_t caml_alloc_string_internal(size_t length) {
     (void)length;
     // TODO: 等待 memory.c 实现
     return CAML_UNIT;
 }
 
 /*==============================================================================
  * 调试函数实现
  *============================================================================*/
 
 void Caml_debug_print(caml_value_t v, char* out, size_t out_size) {
     // 简单的调试输出实现
     char* p = out;
     size_t remaining = out_size;
     
     if (Caml_is_int(v)) {
         int n = snprintf(p, remaining, "%ld", (long)Caml_int_val(v));
         p += n;
         remaining -= n;
     } else {
         int n = snprintf(p, remaining, "<ptr:%p>", Caml_ptr_val(v));
         p += n;
         remaining -= n;
     }
     
     if (remaining > 0) {
         *p = '\0';
     } else {
         out[out_size - 1] = '\0';
     }
 }
 
 void Caml_debug_print_uart(caml_value_t v) {
     // TODO: 等待串口模块实现
     (void)v;
 }