/**
 * @file ffi/ffi.h
 * @brief C primitive 注册与调度 (Phase 4)
 *
 * Primitive 表: 数组索引。C_CALLN 通过索引查找函数指针并调用。
 * 每个 primitive 的签名: value f(value arg1, ..., value argN)。
 */
#ifndef CAMELINO_FFI_H
#define CAMELINO_FFI_H

#include "value.h"

/* Primitive 函数指针类型: 变参, 最多 5 个固定参数 */
typedef value (*caml_prim_t)(value);
typedef value (*caml_prim2_t)(value, value);
typedef value (*caml_prim3_t)(value, value, value);

/* 最大 primitive 数量 */
#define CAML_MAX_PRIMITIVES 256

/* API */
void  caml_init_primitives(void);
int   caml_register_primitive(const char* name, void* func, int arity);
void* caml_lookup_primitive(const char* name);
void* caml_lookup_primitive_by_index(int index, int* arity);

/* 内置 primitive 注册 */
void caml_register_builtin_primitives(void);

#endif
