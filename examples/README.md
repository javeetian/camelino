# Camelino 示例

所有示例遵循相同的流水线：**PC 端编译 `.ml` → `ocamlc` → `camelino-embed` → 字节码 → 设备/主机执行**。

```
.ml 源码  ──→  ocamlc  ──→  .cmo  ──→  camelino-embed  ──→  bytecode.h / .camel
                                                                    │
                                                    ┌───────────────┘
                                                    ▼
                                          设备端/主机: #include + 加载 + 执行
```

设备端**不包含 OCaml 编译器**——所有源码编译在 PC 端完成。

## 目录结构

```
examples/
├── arduino/           # Arduino (RP2350) 平台
│   ├── Blink/         # LED 闪烁 — 最简入门
│   ├── SerialHello/   # 串口输出
│   └── AnalogRead/    # ADC 读取 + 串口打印
├── host/              # 主机模拟器 (Linux/macOS)
│   ├── Hello/         # 终端输出
│   └── Fib/           # Fibonacci 计算 (可与 ocamlrun 差分比对)
└── README.md
```

## Arduino 示例

每个示例包含三个文件：

| 文件 | 用途 |
|------|------|
| `*.ml` | OCaml 源码 |
| `build.sh` | 编译脚本: `.ml` → `bytecode.h` |
| `*.ino` | Arduino 工程 (`#include "bytecode.h"`) |

### 使用方法

```bash
# 1. 编译 OCaml → 生成 bytecode.h
cd examples/arduino/Blink
bash build.sh

# 2. 在 Arduino IDE 中:
#    - 打开 Blink/ 目录
#    - 选择开发板: Raspberry Pi Pico 2 (RP2350)
#    - 编译 + 烧录

# 3. 打开串口监视器 (115200 baud)，观察 LED 闪烁
```

### Blink

经典的 LED 闪烁，使用 OCaml 递归循环。

**硬件:** Pico 2 板载 LED (GPIO 25)

### SerialHello

通过串口输出字符串。

### AnalogRead

从 ADC 引脚读取传感器数据，通过串口输出。

**硬件:** 传感器接 A0 (GPIO 26 on RP2350)

## 主机示例

主机模拟器在 PC 上直接运行字节码，用于开发调试和差分测试。

```bash
# 编译并运行
cd examples/host/Hello
bash build.sh
./hello

# 差分验证 (确认 Camelino 结果与 ocamlrun 一致)
cd examples/host/Fib
bash build.sh
./fib                              # Camelino 输出
ocamlc -o fib.cmo fib.ml && ocamlrun fib.cmo  # 官方输出 (应一致)
```

### Hello

最简单的终端输出 — 验证工具链完整。

### Fib

Fibonacci 计算 — 可与 `ocamlrun` 差分比对，验证解释器正确性。

## 添加新示例

1. 创建 `examples/<platform>/<Name>/` 目录
2. 编写 `*.ml` — 使用 `lib/camelino/` 中声明的 `external` 函数
3. 编写 `build.sh` — 参考现有脚本
4. 编写 `.ino` (Arduino) 或 `main.c` (host) — 加载并执行字节码

## 当前限制

`camelino-embed` 工具尚处于原型阶段，未完整实现 `.cmo` 文件的 Marshal 反序列化（Design.md §3.2.1 任务 1.6.1）。这意味着：

- **已可用**: 纯算术/常量程序（参见 `tools/test_suite/` 差分测试，14 项 C 单元测试全通过）
- **待 Marshal 解析完成后可用**: 含函数/闭包/模块的复杂 OCaml 程序
- Arduino 示例的 `.ino` 文件和 OCaml 源码已就绪，`build.sh` 脚本可生成 `.camel`/`bytecode.h`，待工具链完善后即可端到端烧录运行
