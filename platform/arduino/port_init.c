/**
 * @file platform/arduino/port_init.c
 * @brief Arduino 平台入口 — setup() / loop() 驱动 Camelino
 *
 * 用法：
 *   在 .ino 文件中 #include <Camelino.h>，在 setup() 中调用 Camelino_begin()。
 *   Camelino.h/cpp 引用此文件提供的初始化函数。
 */

#include <Arduino.h>

extern void caml_memory_init(void);
extern void caml_init_primitives(void);
extern void caml_load_bytecode(void);
extern void caml_init_globals(void);
extern void caml_interpret(void);
extern void caml_process_events(void);

/* ---- 启动（由 Camelino_begin() 调用） ---- */

void camelino_port_init(void) {
    caml_memory_init();
    caml_init_primitives();
    caml_load_bytecode();
    caml_init_globals();
}

int camelino_port_step(void) {
    caml_process_events();
    /* caml_interpret() 在安全点返回 */
    caml_interpret();
    return 0;
}
