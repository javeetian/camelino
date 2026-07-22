# Camelino 验证步骤（Phase 0–6）

## 前置条件

| 工具 | 版本 | 安装 |
|------|------|------|
| GCC | ≥ 12 | macOS: `brew install gcc` / Linux: `apt-get install gcc` |
| CMake | ≥ 3.16 | macOS: `brew install cmake` / Linux: `apt-get install cmake` |
| Ninja | 任意 | macOS: `brew install ninja` / Linux: `apt-get install ninja-build` |
| OCaml | 4.14.x | `opam switch create 4.14.2` |
| ocamlfind | 任意 | `opam install ocamlfind` |

## 快速全量验证

```bash
# Linux/macOS: 一键全过
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build
ctest --test-dir build --output-on-failure        # 14 项 C 单元测试
bash tools/test_suite/run_diff.sh                  # 3 项快速差分测试
bash tools/test_suite/run_diff_full.sh             # 41 项全量差分测试
bash tools/test_suite/run_all.sh                   # 全量回归 (含语法检查)
```

---

## Phase 1：值表示 + ZAM 虚拟机

### 1.1 值表示 — test_value

**验证内容：** `Val_long`/`Long_val`/`Is_long`/`Is_block`/`Field`/`Hd_val`/header。

```bash
./build/test_value
```

### 1.2 内存分配器 — test_memory

**验证内容：** 静态堆 bump 分配、值栈 push/pop、全局变量 set/get。

```bash
./build/test_memory
```

### 1.3 ZAM 指令定义 — test_opcodes

**验证内容：** 149 条 ZAM 指令枚举值与 OCaml 4.14 对齐。

```bash
./build/test_opcodes
```

### 1.4 ZAM 解释器 — test_vm

**验证内容：** 手工构造字节码 → `caml_interpret()` → 比对 acc。

```bash
./build/test_vm
```

### 1.5 字节码加载器 — test_bytecode

**验证内容：** `.camel` 格式解析 + CRC32 校验。

```bash
./build/test_bytecode
```

### 1.6 camelino-embed 端到端 — test_embed

```bash
./build/test_embed
```

### 1.7 差分测试

```bash
cd tools/test_suite && bash run_diff.sh
```

---

## Phase 2：函数 + 控制流

### 2.1-2.2 闭包 + 函数调用 — test_calls

```bash
./build/test_calls
```

### 2.3 异常处理 — test_exn

```bash
./build/test_exn
```

### 2.4 全局变量 — test_globals

```bash
./build/test_globals
```

---

## Phase 3：GC + 堆块

### 3.1 Mark-Sweep GC — test_gc

```bash
./build/test_gc
```

---

## Phase 4：FFI + HAL

### 4.1 Primitive 调度 — test_ffi

```bash
./build/test_ffi
```

### 4.2 GC 根保护 — test_gc_roots

```bash
./build/test_gc_roots
```

### 4.3 综合 HAL — test_phase4

```bash
./build/test_phase4
```

---

## Phase 5：完整 ZAM + stdlib

### 5.1 P1 指令 — test_phase5

```bash
./build/test_phase5
```

---

## Phase 6：生态工具 + 多平台移植

### 6.1 camelino-check 静态分析

```bash
# 编译
cd tools/camelino-check && dune build && cd ../..

# 使用
tools/camelino-check.sh my_app.cmo --heap 192k --word-size 32 --platform arduino
```

### 6.2 主机模拟器增强

```bash
# 编译 camelino-host (手动单步)
echo '#define _DARWIN_C_SOURCE' > /tmp/prefix.h
cat /tmp/prefix.h \
    src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c \
    src/core/bytecode.c src/core/gc_roots.c \
    src/ffi/ffi.c src/ffi/primitives.c \
    platform/host/hal_adapter.c platform/host/port_init.c > /tmp/host.c
gcc /tmp/host.c -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi \
    -std=gnu99 -DCAMELINO_HAS_FFI -o build/camelino-host

# 运行
./build/camelino-host --version
./build/camelino-host --acc-only file.camel
./build/camelino-host --trace file.camel
./build/camelino-host --gc-stats --heap-stats file.camel
```

### 6.3 全量差分测试（41 项）

```bash
bash tools/test_suite/run_diff_full.sh
```

预期：41 passed, 0 failed。

覆盖：算术(17) / 比较(8) / 分支(3) / 栈(2) / 全局(2) / 异常(2) / FFI(1) / P1指令(6)。

### 6.4 Arduino 平台适配器（语法验证）

```bash
# Arduino 适配器依赖 <Arduino.h>，无法在主机编译
# 验证语法:
gcc -fsyntax-only platform/arduino/hal_adapter.c -Isrc/core -Iplatform/host -Isrc -Isrc/hal -std=gnu99 -include stdint.h -include stdbool.h -include stddef.h 2>&1

# 在 Arduino IDE 中编译: 打开 examples/Blink/Blink.ino → 选择 Board → Verify
```

### 6.5 REPL 子系统

```bash
# 编译 PC 端 REPL 工具
cd tools/camelino-repl && dune build && cd ../..

# 测试 REPL（local 模式）
echo '2 + 3' | tools/camelino-repl/_build/default/main.exe --local

# 设备端 C 文件语法验证
gcc -fsyntax-only src/repl/line_editor.c -Isrc/core -Iplatform/host -std=gnu99
gcc -fsyntax-only src/repl/history.c -Isrc/core -Iplatform/host -std=gnu99
gcc -fsyntax-only src/repl/repl.c -Isrc/core -Iplatform/host -Isrc -Isrc/hal -std=gnu99
```

### 6.6 构建系统集成

```bash
# camelino deploy
chmod +x tools/camelino-deploy.sh
./tools/camelino-deploy.sh --help

# PlatformIO
pio run              # 编译
pio run -t upload    # 烧录

# CI (本地模拟)
# GitHub Actions: .github/workflows/ci.yml
```

### 6.7 文档

| 文件 | 验证方式 |
|------|---------|
| README.md | 手动审阅 |
| QUICKSTART.md | 按步骤逐一执行 |
| PORTING.md | 对照 platform/arduino/ 检查 |
| CONTRIBUTING.md | 对照实际开发流程 |
| COMPATIBILITY.md | 运行 camelino-check 交叉验证 |

### 6.8 OPAM 库兼容性验证

```bash
# 编译测试程序
for f in /tmp/cml_test*.ml; do
    ocamlc -c -o "${f%.ml}.cmo" "$f"
    tools/camelino-check.sh "${f%.ml}.cmo" --heap 192k --word-size 32 --platform arduino
done
```

### 6.9 C Stub 嵌入

```bash
# 扫描 C stub
./tools/camelino-stub.sh scan tools/test_stub.c

# 生成 Arduino wrapper
./tools/camelino-stub.sh wrap tools/test_stub.c -o /tmp/stubs.h

# 完整嵌入
./tools/camelino-stub.sh embed tools/test_stub.c --bytecode file.camel -o /tmp/bundle.h
```

---

## 语法批量验证

所有 26 个源文件的一次性语法检查：

```bash
for f in src/core/*.c src/ffi/*.c src/bindings/*.c src/repl/*.c test/test_*.c; do
  printf "%-45s" "${f##*/}: "
  gcc -fsyntax-only $f -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi -std=gnu99 2>&1 && echo "OK" || echo "FAIL"
done
```

预期 26 个文件全部 OK。

---

## 全量回归脚本 (run_all.sh)

```bash
#!/bin/bash
# tools/test_suite/run_all.sh — 一键全量回归
set -e
echo "=== C Unit Tests (14) ==="
ctest --test-dir build --output-on-failure
echo ""
echo "=== Quick Diff Tests (3) ==="
bash tools/test_suite/run_diff.sh
echo ""
echo "=== Full Diff Tests (41) ==="
bash tools/test_suite/run_diff_full.sh
echo ""
echo "=== Syntax Check (26 files) ==="
PASS=0; FAIL=0
for f in src/core/*.c src/ffi/*.c src/bindings/*.c src/repl/*.c test/test_*.c; do
  if gcc -fsyntax-only $f -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi -std=gnu99 2>/dev/null; then
    PASS=$((PASS+1))
  else
    echo "FAIL: $f"; FAIL=$((FAIL+1))
  fi
done
echo "Syntax: $PASS passed, $FAIL failed"
echo ""
echo "=== OCaml Tools ==="
(cd tools/camelino-embed && ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml 2>/dev/null && echo "camelino-embed: OK") || echo "camelino-embed: FAIL"
(cd tools/camelino-check && dune build 2>/dev/null && echo "camelino-check: OK") || echo "camelino-check: FAIL"
(cd tools/camelino-repl && dune build 2>/dev/null && echo "camelino-repl: OK") || echo "camelino-repl: FAIL"
echo ""
echo "=== All Done ==="
```

---

## 平台说明

| 平台 | cmake + ctest | 差分测试 | 语法验证 | OCaml 工具 |
|------|--------------|---------|---------|-----------|
| Linux x86-64 | ✓ 14/14 | ✓ 41/41 | ✓ 26/26 | ✓ |
| macOS ARM64 | ✓ 14/14 | ✓ 41/41 | ✓ 26/26 | ✓ |
| Windows MinGW | 仅前 3 个 | ✓ | ✓ | ✓ |
| Arduino (RP2350) | 交叉编译 | — | — | — |

## 测试汇总

| Phase | 测试项 | 数量 | 命令 |
|-------|--------|------|------|
| 1 | C 单元测试 | 14 | `ctest --test-dir build` |
| 1 | 快速差分测试 | 3 | `bash tools/test_suite/run_diff.sh` |
| 1-5 | 全量差分测试 | 41 | `bash tools/test_suite/run_diff_full.sh` |
| 6.1 | camelino-check | 9 程序 | `tools/camelino-check.sh` |
| 6.2 | host simulator | 4 flags | `build/camelino-host --help` |
| 6.5 | REPL local | 1 | `echo '2+3' \| camelino-repl --local` |
| 6.9 | C stub scan | 1 | `tools/camelino-stub.sh scan` |
| 全部 | 语法检查 | 26 文件 | `gcc -fsyntax-only` |
