/*
 * AnalogRead.ino — Camelino ADC 读取示例
 *
 * 前置步骤: 运行 bash build.sh 生成 bytecode.h
 * 硬件: 传感器接 A0 (GPIO 26 on RP2350)
 */

#include <Arduino.h>
extern "C" {
#include "core/vm.h"
#include "core/memory.h"
#include "ffi/ffi.h"
}
#include "bytecode.h"

void setup() {
    Serial.begin(115200);
    while (!Serial);

    camelino_vm_init();
    caml_init_primitives();
    caml_startup(camelino_code, camelino_code_size);
    caml_interpret();
}

void loop() {
    if (!caml_vm_halted()) caml_interpret();
}
