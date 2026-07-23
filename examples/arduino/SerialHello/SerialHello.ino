/*
 * SerialHello.ino — Camelino 串口输出示例
 *
 * 前置步骤: 运行 bash build.sh 生成 bytecode.h
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

    Serial.println("Done.");
}

void loop() {
    if (!caml_vm_halted()) caml_interpret();
}
