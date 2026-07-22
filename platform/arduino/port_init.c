/**
 * @file platform/arduino/port_init.c
 * @brief Arduino 平台入口 — setup()/loop() 驱动 Camelino 运行时
 *
 * 调度模型 (§9.4):
 *   setup() → 初始化内存/primitive/字节码/全局变量
 *   loop()  → caml_process_events() (轮询 ISR flag → caml_callback)
 *           → caml_interpret() (安全点 yield，回到 loop)
 *
 * 与 platform/host 的区别:
 *  - host: main() 一次性执行到 STOP，输出结果后退出
 *  - arduino: 持续事件循环，永不退出，适合嵌入式长生命周期
 *
 * 用法:
 *   在 .ino 文件中:
 *     #include <Camelino.h>
 *     void setup() { Serial.begin(115200); Camelino_begin(); }
 *     void loop()  { Camelino_task(); }
 */

#include <Arduino.h>
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
#include "value.h"

/* ---- 外部声明 ---- */
extern void caml_init_primitives(void);
extern void caml_init_globals(void);
extern void caml_process_events(void);

/* 由 camelino-embed 生成的 bytecode.h 提供:
 *   camelino_code[], camelino_code_size, camelino_data[], camelino_data_size
 * 如果不使用预编译嵌入（改用串口加载），这些符号不存在，
 * 编译时由 CAMELINO_EMBED_BYTECODE 宏控制。
 */
#ifdef CAMELINO_EMBED_BYTECODE
extern const uint8_t camelino_code[];
extern const size_t  camelino_code_size;
extern const uint8_t camelino_data[];
extern const size_t  camelino_data_size;
#endif

static int halted = 0;

/* ---- 初始化 (由 Camelino_begin() 调用) ---- */

void camelino_port_init(void) {
    caml_memory_init();
    caml_init_primitives();

#ifdef CAMELINO_EMBED_BYTECODE
    /* 预编译模式: 字节码嵌入 Flash (PROGMEM) */
    int rc = caml_bytecode_load(camelino_code, camelino_code_size);
    if (rc != CAML_BC_OK) {
        Serial.print("FATAL: bytecode load failed, code=");
        Serial.println(rc);
        return;
    }
    caml_load_bytecode_buf(
        caml_bytecode_code(), caml_bytecode_code_size(),
        camelino_data, camelino_data_size);
#else
    /* 串口加载模式: 字节码由 REPL 或 camelino-repl 工具通过串口发送 */
    /* 在 caml_repl_process() 中按需加载 */
#endif

    caml_init_globals();
    halted = 0;
}

/* ---- 主循环步进 (由 Camelino_task() → loop() 调用) ---- */

int camelino_port_step(void) {
    /* 1. 处理事件: 轮询 ISR flag，触发 OCaml 回调 */
    caml_process_events();

    /* 2. 推进 VM: 在安全点 yield 回到 loop()，或 STOP 后永久挂起 */
    if (!halted) {
        caml_interpret();
        halted = caml_vm_halted();
    }

    /* 3. GC 安全点: 与 yield 复用计数器 (§6.3/§9.4) */
    /* caml_interpret() 内部通过 yield_counter 自动处理 */

    return halted ? 1 : 0;
}

/* ---- 便利函数 ---- */

void camelino_port_reset(void) {
    halted = 0;
    caml_vm_init();
    caml_init_primitives();
    caml_init_globals();
}

int camelino_port_is_halted(void) {
    return halted;
}
