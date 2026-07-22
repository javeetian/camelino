# Camelino 平台移植指南

如何将 Camelino 移植到新的目标平台。

## 概述

Camelino 的运行时核心 (`src/core/`) 是平台无关的 C99 代码。移植一个新平台只需：

1. 实现 HAL 接口函数
2. 提供平台配置文件
3. 提供入口函数

**核心运行时零改动** — 这是 HAL 解耦设计的核心目标。

## 移植步骤

### 1. 创建平台目录

```bash
mkdir -p platform/myplatform
```

### 2. 编写 platform_config.h

```c
// platform/myplatform/platform_config.h
#ifndef CAMELINO_PLATFORM_CONFIG_H
#define CAMELINO_PLATFORM_CONFIG_H

// 字长 (32 或 64)
#define CAMELINO_WORD_SIZE   32

// 字节序 (0=小端, 1=大端)
#define CAMELINO_BIG_ENDIAN  0

// FPU (0=软浮点, 1=硬浮点)
#define CAMELINO_HAS_FPU     0

// 内存池大小
#define HEAP_SIZE      (192 * 1024)   // GC 堆
#define STACK_SIZE     8192           // ZAM 值栈 (slot 数)
#define MAX_GLOBALS    4096           // 全局变量槽位数

#endif
```

### 3. 实现 HAL 接口 (hal_adapter.c)

必须实现以下头文件中声明的全部函数：

| 头文件 | 函数 | 必需? |
|--------|------|------|
| `hal/gpio.h` | `hal_gpio_init`, `hal_gpio_write`, `hal_gpio_read`, `hal_gpio_toggle` | ✅ 必需 |
| `hal/uart.h` | `hal_uart_init`, `hal_uart_write`, `hal_uart_read`, `hal_uart_flush` | ✅ 必需 |
| `hal/time.h` | `hal_time_millis`, `hal_time_micros`, `hal_delay_ms`, `hal_delay_us` | ✅ 必需 |
| `hal/err.h` | — (仅类型定义) | — |
| `hal/cap.h` | `hal_cap_supported` | ✅ 必需 |
| `hal/analog.h` | `hal_adc_init`, `hal_adc_read`, `hal_adc_resolution`, `hal_pwm_init`, `hal_pwm_write` | 可选 (返回 UNSUPPORTED) |
| `hal/flash.h` | `hal_flash_read`, `hal_flash_write`, `hal_flash_erase` | 可选 |
| `hal/interrupt.h` | `hal_gpio_attach_isr`, `hal_gpio_detach_isr`, `hal_isr_flag_take` | 可选 |

不支持的函数返回 `HAL_ERR_UNSUPPORTED`。

### 4. 编写入口 (port_init.c)

```c
// platform/myplatform/port_init.c
#include "vm.h"
#include "memory.h"

extern void caml_init_primitives(void);
extern void caml_init_globals(void);
extern void caml_process_events(void);

void myplatform_port_init(void) {
    caml_memory_init();
    caml_init_primitives();
    // 加载字节码
    caml_init_globals();
}

void myplatform_port_loop(void) {
    caml_process_events();
    caml_interpret();
}
```

### 5. 声明支持的能力 (hal_cap_supported)

```c
bool hal_cap_supported(hal_cap_id_t cap) {
    switch (cap) {
    case HAL_CAP_GPIO:      return true;   // 已实现
    case HAL_CAP_UART:      return true;   // 已实现
    case HAL_CAP_ADC:       return false;  // 未实现
    case HAL_CAP_FLASH:     return false;  // 未实现
    // ...
    default: return false;
    }
}
```

### 6. 更新 CMakeLists.txt

在 `CMakeLists.txt` 中添加平台分支：

```cmake
elseif(CAMELINO_PLATFORM STREQUAL "myplatform")
    add_definitions(-DCAMELINO_PLATFORM_MYPLATFORM)
    set(PLATFORM_DIR ${CAMELINO_ROOT}/platform/myplatform)
endif()
```

## 参考实现

| 平台 | 文件 | 特点 |
|------|------|------|
| [host](platform/host/) | hal_adapter.c + port_init.c | libc/POSIX 模拟，GPIO→日志，Flash→文件，用于差分测试 |
| [arduino](platform/arduino/) | hal_adapter.c + port_init.c | 真实硬件 API，Arduino-Pico core，RP2350 首期目标 |

两个适配器的对比展示了 HAL 解耦的核心价值：**相同的 HAL 接口，完全不同的底层实现**。

## ISR 约束 (§7.5)

中断服务例程中**禁止**调用：
- VM 函数 (`caml_interpret`, `caml_callback`)
- FFI / 分配函数 (`caml_alloc`)
- GC 触发函数
- 任何 `caml_*` 函数

ISR 只能设置 `volatile` flag。主循环在安全点轮询 flag → 通过 `caml_callback` 触发 OCaml 处理。

## 验证

移植完成后运行差分测试套件验证：

```bash
cmake --build build
ctest --test-dir build
bash tools/test_suite/run_diff_full.sh
```
