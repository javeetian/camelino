// camelino_repl.h
#ifndef CAMELINO_REPL_H
#define CAMELINO_REPL_H

#include "camelino_vm.h"

// REPL 配置
typedef struct {
    int uart_id;        // 串口编号（默认 0）
    int baud_rate;      // 波特率（默认 115200）
    int echo_enabled;   // 是否回显
    int history_size;   // 命令历史大小
} caml_repl_config_t;

// REPL 状态
typedef struct {
    caml_vm_t* vm;                  // 关联的 VM
    char line_buffer[256];          // 当前行缓冲区
    int line_pos;                   // 缓冲区位置
    char** history;                 // 命令历史
    int history_count;              // 历史条目数
    int history_index;              // 当前浏览位置
    int multiline_mode;             // 多行模式
} caml_repl_t;

// REPL API
void caml_repl_init(caml_repl_t* repl, caml_vm_t* vm, const caml_repl_config_t* config);
void caml_repl_task(caml_repl_t* repl);  // 在 loop() 中周期性调用
void caml_repl_eval_line(caml_repl_t* repl, const char* line);

#endif