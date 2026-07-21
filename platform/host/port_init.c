/**
 * @file platform/host/port_init.c
 * @brief 主机模拟器入口 — main() 驱动 Camelino
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- runtime API（Phase 1+ 由 core/ 实现，当前为弱桩） ---- */

int  caml_main_loop_running = 0;

void __attribute__((weak)) caml_memory_init(void)      { fprintf(stderr, "[stub] caml_memory_init\n"); }
void __attribute__((weak)) caml_init_primitives(void)  { fprintf(stderr, "[stub] caml_init_primitives\n"); }
int  __attribute__((weak)) caml_load_bytecode(const uint8_t* data, size_t len)
    { (void)data; (void)len; fprintf(stderr, "[stub] caml_load_bytecode\n"); return 0; }
void __attribute__((weak)) caml_init_globals(void)     { fprintf(stderr, "[stub] caml_init_globals\n"); }
void __attribute__((weak)) caml_interpret(void)        { fprintf(stderr, "[stub] caml_interpret\n"); caml_main_loop_running = 0; }
void __attribute__((weak)) caml_process_events(void)   { /* no-op */ }

/* ---- 默认嵌入字节码（由 camelino-embed 生成） ---- */

#if 0 /* Phase 1 启用 */
#include "bytecode.h"
#else
static const uint8_t placeholder_code[1] = { 0 };  /* no bytecode yet */
#define camelino_code      placeholder_code
#define camelino_code_size 0
#define camelino_data      NULL
#define camelino_data_size 0
#endif

/* ---- 入口 ---- */

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    caml_memory_init();
    caml_init_primitives();

    /* 加载字节码 */
    if (camelino_code_size > 0) {
        if (caml_load_bytecode(camelino_code, camelino_code_size) != 0) {
            fprintf(stderr, "FATAL: bytecode validation failed\n");
            return 1;
        }
    }

    caml_init_globals();

    /* 主循环：推进解释器 + 处理事件 */
    caml_main_loop_running = 1;
    while (caml_main_loop_running) {
        caml_process_events();
        caml_interpret();
    }

    fprintf(stderr, "[camelino-host] VM halted, exiting\n");
    return 0;
}
