#include "Camelino.h"
#include "core/vm.h"
#include "ffi/ffi.h"

// 串口 REPL 缓冲区
#define REPL_BUF_SIZE 256
static char repl_buffer[REPL_BUF_SIZE];
static size_t repl_index = 0;

// 注册所有 Arduino API 到 OCaml
static void register_all_functions() {
    Camelino_register_function("pin_mode", (Camelino_CFunction)camelino_pin_mode);
    Camelino_register_function("digital_write", (Camelino_CFunction)camelino_digital_write);
    Camelino_register_function("digital_read", (Camelino_CFunction)camelino_digital_read);
    Camelino_register_function("analog_read", (Camelino_CFunction)camelino_analog_read);
    Camelino_register_function("analog_write", (Camelino_CFunction)camelino_analog_write);
    Camelino_register_function("delay", (Camelino_CFunction)camelino_delay_ms);
    Camelino_register_function("millis", (Camelino_CFunction)camelino_millis);
}

void Camelino_begin() {
    Serial.begin(115200);
    while (!Serial);  // 等待串口连接（USB 需要）
    
    camelino_vm_init();           // 初始化 Camelino 虚拟机
    register_all_functions(); // 注册 API 绑定
    
    Serial.println("Camelino v0.1.0 ready");
    Serial.println("Type OCaml code, end with ';;'");
}

int Camelino_eval(const char* code) {
    return camelino_vm_eval_string(code);
}

void Camelino_repl_task() {
    // 从串口读取字符，构建一行代码
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (repl_index > 0) {
                repl_buffer[repl_index] = '\0';
                
                // 检查是否以 ;; 结尾
                if (repl_index >= 2 && 
                    repl_buffer[repl_index-2] == ';' && 
                    repl_buffer[repl_index-1] == ';') {
                    
                    Serial.print("> ");
                    int ret = Camelino_eval(repl_buffer);
                    if (ret != 0) {
                        Serial.println("Error evaluating code");
                    }
                    Serial.print("Camelino> ");
                } else {
                    // 继续读取多行输入
                    Serial.print("... ");
                }
                repl_index = 0;
            }
        } else if (c == '\b' || c == 0x7f) {  // 退格处理
            if (repl_index > 0) repl_index--;
        } else if (repl_index < REPL_BUF_SIZE - 1) {
            repl_buffer[repl_index++] = c;
            Serial.print(c);  // 回显
        }
    }
}

// 示例：点灯函数（可被 OCaml 调用）
void camelino_pin_mode(int pin, int mode) {
    pinMode(pin, mode);
}

void camelino_digital_write(int pin, int value) {
    digitalWrite(pin, value);
}

// ... 其他 API 实现