# Camelino — 可移植 OCaml 字节码运行时

[![CI](https://github.com/javeetian/camelino/actions/workflows/ci.yml/badge.svg)](https://github.com/javeetian/camelino/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![OCaml 4.14](https://img.shields.io/badge/OCaml-4.14.x-orange.svg)](https://ocaml.org)

Camelino 是一个**可移植的 OCaml 字节码运行时**，让你在微控制器上运行标准 `ocamlc` 生成的字节码。

首期目标平台 **Raspberry Pi Pico 2 (RP2350)**，通过硬件抽象层 (HAL) 可移植到 Arduino / Pico SDK / ESP-IDF / Zephyr / 裸机 / 主机模拟器。

```
写 OCaml 代码 → ocamlc 编译 → camelino-embed 嵌入 → 烧录 → 在 MCU 上运行
```

## 快速开始

### 1. 安装 OCaml 4.14

```bash
opam switch create 4.14.2
eval $(opam env)
```

### 2. 编译并测试 (主机模拟器)

```bash
# 编译
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build

# 运行测试 (14 项 C 单元测试 + 41 项差分测试)
ctest --test-dir build --output-on-failure
bash tools/test_suite/run_diff_full.sh
```

### 3. 编译 OCaml 程序 → 设备

```bash
# 编译 OCaml → bytecode
echo 'let rec loop () = ignore 42; loop () in loop ()' > my_app.ml
ocamlc -c -o my_app.cmo my_app.ml

# 嵌入为 C 头文件
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed.exe main.ml
./camelino-embed.exe ../../my_app.cmo -o bytecode.h

# Arduino IDE: #include "bytecode.h", 烧录到 RP2350
```

## 架构

```
 PC 端                                设备端 (MCU)
 ┌──────────┐                        ┌─────────────────────┐
 │ .ml 源码  │                        │  ZAM 字节码解释器    │
 │    ↓     │                        │  (125+ 条指令)      │
 │ ocamlc   │    bytecode.h          │    ↓                │
 │    ↓     │ ──────────────────→    │  Mark-Sweep GC      │
 │ camelino │    (烧录到 Flash)       │    ↓                │
 │ -embed   │                        │  FFI / C primitive  │
 └──────────┘                        │    ↓                │
                                     │  硬件抽象层 (HAL)    │
                                     │    ↓                │
                                     │  平台适配器          │
                                     │  (Arduino/PicoSDK/) │
                                     └─────────────────────┘
```

## 项目结构

| 目录 | 内容 |
|------|------|
| `src/core/` | ZAM 虚拟机、值表示、内存管理、GC |
| `src/ffi/` | C primitive 注册与调度 |
| `src/bindings/` | OCaml↔HAL 桥接 (CAMLprim) |
| `src/hal/` | 硬件抽象层接口 (平台无关) |
| `src/repl/` | 设备端 REPL (行编辑/历史/字节码加载) |
| `platform/host/` | 主机模拟器 (差分测试用) |
| `platform/arduino/` | Arduino 平台适配器 (RP2350) |
| `tools/camelino-embed/` | .cmo→.camel 转换工具 |
| `tools/camelino-check/` | 静态兼容性分析 |
| `tools/camelino-repl/` | PC 端交互式 REPL |
| `tools/test_suite/` | 差分测试套件 (41 项) |
| `lib/camelino/` | OCaml 端 FFI 声明库 |
| `examples/` | Blink / REPL / Sensor 示例 |

## 测试

```bash
# 全部测试
cmake --build build && ctest --test-dir build --output-on-failure
bash tools/test_suite/run_diff_full.sh

# 单独测试
./build/test_vm.exe       # ZAM 解释器
./build/test_gc.exe       # 垃圾回收
./build/test_ffi.exe      # FFI 调度
```

## 文档

| 文档 | 内容 |
|------|------|
| [Design.md](Design.md) | 架构设计 (v0.4.0) |
| [Work.md](Work.md) | 工作计划 (Phase 0–6) |
| [BUILD.md](BUILD.md) | 编译与测试指南 |
| [TEST.md](TEST.md) | 验证步骤 |
| [QUICKSTART.md](QUICKSTART.md) | 快速开始 |
| [PORTING.md](PORTING.md) | 平台移植指南 |
| [CONTRIBUTING.md](CONTRIBUTING.md) | 贡献指南 |

## 许可证

MIT © 2025–2026
