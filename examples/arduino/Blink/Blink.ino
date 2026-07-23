/*
 * Blink.ino — Camelino 字节码点灯示例
 *
 * 前置步骤: 运行 bash build.sh 生成 bytecode.h
 *
 * 工作原理:
 *   1. PC 端: ocamlc → camelino-embed → bytecode.h
 *   2. 设备端: #include "bytecode.h", 加载预编译字节码, 执行
 *
 * 硬件: Raspberry Pi Pico 2 (RP2350), 板载 LED = GPIO 25
 */

#include <Arduino.h>

/* ---- Camelino 运行时头文件 ---- */
extern "C" {
#include "core/vm.h"
#include "core/memory.h"
#include "ffi/ffi.h"
}

/* ---- 由 build.sh 生成的字节码 ---- */
#include "bytecode.h"

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Camelino Blink — running OCaml bytecode on RP2350");

    /* 初始化运行时, 加载字节码, 执行全局初始化 */
    camelino_vm_init();
    caml_init_primitives();
    caml_startup(camelino_code, camelino_code_size);

    /* 运行 ZAM 解释器 — 对于死循环程序, caml_interpret() 在 yield 安全点返回 */
    caml_interpret();

    /* 如果程序退出了 (不应发生 — blink 是死循环) */
    Serial.println("Blink exited.");
}

void loop() {
    /* 处理 GPIO 中断事件 (如有) */
    /* caml_process_events(); */

    /* 如果 VM 因 yield 暂停, 恢复执行 */
    if (!caml_vm_halted()) {
        caml_interpret();
    }
}
