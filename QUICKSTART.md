# Camelino 快速开始

5 分钟内从零到在 MCU 模拟器上运行 OCaml 字节码。

## 前提

- macOS / Linux / Windows (WSL)
- GCC ≥ 12、CMake ≥ 3.16、Ninja
- OCaml 4.14.x (`opam switch create 4.14.2`)

## 1. 安装依赖

```bash
# macOS
brew install cmake ninja gcc

# Linux (Ubuntu/Debian)
sudo apt-get install -y gcc cmake ninja-build

# OCaml
opam switch create 4.14.2 && eval $(opam env)
```

## 2. 克隆 & 编译

```bash
git clone https://github.com/javeetian/camelino.git
cd camelino

cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build
```

## 3. 运行测试

```bash
# C 单元测试 (14 项 — 值表示、VM、GC、FFI)
ctest --test-dir build --output-on-failure

# 差分测试 (41 项 — 算术、比较、分支、异常、闭包、FFI)
bash tools/test_suite/run_diff_full.sh
```

## 4. 编译 OCaml 程序

```bash
# 写一个 OCaml 程序
echo 'let _ = 2 + 3' > add.ml

# 编译 → .cmo
ocamlc -c -o add.cmo add.ml

# 嵌入 → .camel
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml
./camelino-embed ../../add.cmo -o ../../add.camel
cd ../..
```

## 5. 在主机模拟器上运行

```bash
# 编译 camelino-host
echo '#define _DARWIN_C_SOURCE' > /tmp/prefix.h
cat /tmp/prefix.h \
    src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c \
    src/core/bytecode.c src/core/gc_roots.c \
    src/ffi/ffi.c src/ffi/primitives.c \
    platform/host/hal_adapter.c platform/host/port_init.c \
    > /tmp/host.c
gcc /tmp/host.c -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi \
    -std=gnu99 -DCAMELINO_HAS_FFI -o camelino-host

# 运行
./camelino-host add.camel
```

## 6. 交互式 REPL

```bash
# 启动 REPL
echo '2 + 3' | tools/camelino-repl/_build/default/main.exe --local
```

## 7. 烧录到真实硬件 (RP2350)

```bash
# 编译 OCaml 程序
ocamlc -c -o blink.cmo blink.ml

# 生成 bytecode.h
tools/camelino-embed/camelino-embed blink.cmo -o bytecode.h

# PlatformIO 一键编译 + 烧录
pio run -t upload

# 或 Arduino CLI
arduino-cli compile --fqbn arduino:mbed_rp2040:rpipico2
arduino-cli upload  --fqbn arduino:mbed_rp2040:rpipico2 --port /dev/ttyACM0
```

## 下一步

- [Design.md](Design.md) — 架构设计文档
- [PORTING.md](PORTING.md) — 移植到新平台
- [TEST.md](TEST.md) — 完整验证步骤
