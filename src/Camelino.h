#ifndef CAMELINO_H
#define CAMELINO_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif


// 初始化 Camelino 运行时
void Camelino_begin();

// 执行 OCaml 代码字符串
// 返回 0 表示成功，非 0 表示错误
int Camelino_eval(const char* code);

// 执行 .cmo 字节码文件（通过串口或 SD 卡传入）
int Camelino_execute_bytes(const uint8_t* bytecode, size_t len);

// 从串口 REPL 读取一行并执行
void Camelino_repl_task();

// 注册自定义 C 函数供 OCaml 调用
typedef void (*Camelino_CFunction)();
void Camelino_register_function(const char* name, Camelino_CFunction func);

// === 基础硬件 API 的 OCaml 绑定 ===

// 这些函数会被注册到 OCaml 环境中
void camelino_pin_mode(int pin, int mode);
void camelino_digital_write(int pin, int value);
int camelino_digital_read(int pin);
int camelino_analog_read(int pin);
void camelino_analog_write(int pin, int value);
void camelino_delay_ms(int ms);
unsigned long camelino_millis();

#ifdef __cplusplus
}
#endif

#endif // CAMELINO_H