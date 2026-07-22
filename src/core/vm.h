/**
 * @file core/vm.h
 * @brief ZAM 虚拟机解释器 — 公共 API
 *
 * caml_interpret() — 主解释循环，逐条执行 ZAM 指令直到 yield 或 STOP。
 * 所有状态保存在全局变量中（单实例，嵌入式友好）。
 */

#ifndef CAMELINO_VM_H
#define CAMELINO_VM_H

#include "platform.h"
#include <stddef.h>

/* ---- API ---- */

void caml_vm_init(void);

/* 一站式启动与全局变量初始化 */
void caml_startup(const uint8_t* code, size_t code_size);
void caml_init_globals(void);

/* 加载字节码到 VM */
void caml_load_bytecode_buf(const uint8_t* code, size_t code_size,
                             const uint8_t* data, size_t data_size);

/* 主解释循环（在安全点或 STOP 时返回） */
void caml_interpret(void);

/* 寄存器访问（测试/调试用） */
value caml_get_acc(void);
void  caml_set_acc(value v);
value caml_get_sp(mlsize_t slot);     /* sp[slot] */
value caml_get_global(mlsize_t slot);
void  caml_set_global(mlsize_t slot, value v);
int   caml_vm_halted(void);           /* STOP 后返回 true */

/* ---- 调试/性能 Hook（Phase 6.2 主机模拟器用） ---- */

/* Trace hook: 每执行一条指令时回调。参数：opcode, pc偏移, acc值 */
typedef void (*caml_trace_fn)(uint8_t op, size_t pc_offset, value acc);
void caml_set_trace_hook(caml_trace_fn fn);

/* 指令执行计数 */
size_t caml_get_instruction_count(void);

#endif
