// camelino_ffi.h
#ifndef CAMELINO_FFI_H
#define CAMELINO_FFI_H

#include "camelino_value.h"

// FFI 函数签名
// 参数：vm（VM 上下文）、args（参数数组）、nargs（参数个数）
// 返回值：caml_value_t
typedef caml_value_t (*caml_ffi_func_t)(caml_vm_t* vm, caml_value_t* args, int nargs);

// FFI 注册表
typedef struct {
    const char* name;           // OCaml 中可见的函数名
    caml_ffi_func_t func;       // C 实现
    int arity;                  // 参数个数
} caml_ffi_entry_t;

// FFI API
void caml_ffi_init(void);
int caml_ffi_register(const char* name, caml_ffi_func_t func, int arity);
caml_ffi_func_t caml_ffi_lookup(const char* name);
caml_value_t caml_ffi_call(caml_vm_t* vm, const char* name, caml_value_t* args, int nargs);

#endif