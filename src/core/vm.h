// camelino_vm.h
#ifndef CAMELINO_VM_H
#define CAMELINO_VM_H

#include "camelino_value.h"

// 字节码指令格式
// 变长指令：操作码(1字节) + 操作数(0-4字节)
typedef struct {
    uint8_t* code;          // 字节码段
    size_t code_size;       // 字节码长度
    caml_value_t* constants; // 常量池
    size_t num_constants;   // 常量数
    caml_value_t* globals;   // 全局变量表
    size_t num_globals;     // 全局变量数
} caml_bytecode_t;

// 虚拟机状态
typedef struct {
    // 执行栈
    caml_value_t* stack;        // 栈底
    caml_value_t* sp;           // 栈顶指针
    caml_value_t* stack_limit;  // 栈上限
    
    // 程序计数器
    uint8_t* pc;
    
    // 当前执行的字节码
    caml_bytecode_t* bc;
    
    // 全局环境
    caml_value_t* global_env;
    size_t global_env_size;
    
    // 调用帧链表
    struct caml_frame* frames;
    
    // 异常处理
    struct caml_exn_handler* handlers;
    
    // 统计信息
    uint32_t instr_count;   // 已执行指令数
    uint32_t gc_count;      // GC 次数
} caml_vm_t;

// VM API
int caml_vm_init(caml_vm_t* vm, caml_memory_config_t* mem_config);
int caml_vm_load(caml_vm_t* vm, const caml_bytecode_t* bc);
int caml_vm_run(caml_vm_t* vm, caml_value_t* result);
void caml_vm_reset(caml_vm_t* vm);

#endif