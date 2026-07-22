# Arduino Platform Adapter — RP2350 (arduino-pico)

Camelino 的 Arduino 平台适配器。在 RP2350 (Raspberry Pi Pico 2) + Arduino-Pico core 上运行 OCaml 字节码。

## 前置条件

- [Arduino IDE](https://www.arduino.cc/en/software) 或 [PlatformIO](https://platformio.org/)
- [Arduino-Pico](https://github.com/earlephilhower/arduino-pico) (Earle Philhower's RP2040/RP2350 core)
- 开发板: Raspberry Pi Pico 2 / Pico 2 W

## 快速开始

### 1. 安装 Arduino-Pico

Arduino IDE → File → Preferences → Additional Boards Manager URLs:
```
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```
Tools → Board → Boards Manager → 搜索 "Raspberry Pi Pico" → 安装

### 2. 编译 OCaml → bytecode.h

```bash
# PC 端:
ocamlc -c -o my_app.cmo my_app.ml
tools/camelino-embed/camelino-embed my_app.cmo -o bytecode.h
```

### 3. Arduino IDE 打开示例

- 打开 `examples/Blink/Blink.ino`
- 将 `bytecode.h` 放在同目录下
- Board: Raspberry Pi Pico 2
- Upload

## HAL 能力

| Capability | 状态 | 备注 |
|-----------|------|------|
| GPIO | ✅ | pinMode / digitalWrite / digitalRead |
| UART | ✅ | Serial / Serial1 / Serial2 |
| ADC | ✅ | analogRead (10-bit, 0-1023) |
| PWM | ✅ | analogWrite (8-bit, 0-255) |
| GPIO Interrupt | ✅ | attachInterrupt (ISR 只置 flag) |
| Time (ms) | ✅ | millis() |
| Time (µs) | ✅ | micros() |
| Flash R/W | ❌ | 字节码嵌入 PROGMEM，不支持运行时 Flash 写入 |
| Sleep | ❌ | 未来 |
| I2C | ❌ | 未来 |
| SPI | ❌ | 未来 |

## 内存配置

| 参数 | 值 | 说明 |
|------|-----|------|
| CAMELINO_WORD_SIZE | 32 | RP2350 是 32 位 ARM |
| CAMELINO_BIG_ENDIAN | 0 | 小端 |
| CAMELINO_HAS_FPU | 0 | 软 FPU (Cortex-M33 无 FPU) |
| HEAP_SIZE | 192 KB | OCaml GC 堆 (总 SRAM 520 KB) |
| STACK_SIZE | 8192 slots | ZAM 值栈 |
| MAX_GLOBALS | 4096 | 全局变量槽位数 |

## 与 Host 平台的关键区别

| 特性 | Host | Arduino |
|------|------|---------|
| GPIO | 日志文件 `gpio_state.json` | 真实硬件 pin |
| UART | stdout/stderr FILE* | 硬件 Serial |
| 时间 | gettimeofday | 硬件定时器 millis()/micros() |
| 入口 | main(argc, argv) | setup()/loop() |
| 生命周期 | 执行完退出 | 永久事件循环 |
| Flash | 文件模拟 `flash.bin` | 不支持运行时写入 |
| 调试 | --trace stderr | Serial.print |

## 移植检查清单 (§7.6.4)

- [x] 实现 HAL 接口函数 (gpio/uart/time/analog/interrupt/cap)
- [x] 提供 platform_config.h (字长/字节序/FPU/内存池大小)
- [x] 提供 port_init.c (setup/loop 驱动)
- [ ] 差分测试通过 (需要实际硬件)
- [ ] 在 RP2350 上运行 Blink 示例
