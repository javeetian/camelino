# Camelino 编译与测试指南（Phase 0–6）

## 前置条件

| 工具 | 版本要求 | 安装方式 |
|------|---------|---------|
| GCC | ≥ 12 | macOS: `brew install gcc` / Linux: `apt-get install gcc` |
| CMake | ≥ 3.16 | macOS: `brew install cmake` / Linux: `apt-get install cmake` |
| Ninja | 任意 | macOS: `brew install ninja` / Linux: `apt-get install ninja-build` |
| OCaml | 4.14.x | `opam switch create 4.14.2` |
| ocamlfind | 任意 | `opam install ocamlfind` |
| dune | ≥ 3.8 | `opam install dune` (或随 OCaml 自带) |

---

## 一键编译与测试

```bash
# 1. 配置（仅需一次）
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc

# 2. 编译所有 C 目标（14 个测试 + host 模拟器）
cmake --build build

# 3. 编译 OCaml 工具
cd tools/camelino-embed && ocamlfind ocamlc -linkpkg \
  -package compiler-libs.common -o camelino-embed.exe main.ml && cd ../..
cd tools/camelino-check && dune build main.exe && cd ../..
cd tools/camelino-repl  && dune build main.exe && cd ../..

# 4. 运行全部测试
ctest --test-dir build --output-on-failure        # 14 项 C 单元测试
bash tools/test_suite/run_diff.sh                  # 3 项快速差分测试
bash tools/test_suite/run_diff_full.sh             # 41 项全量差分测试
```

---

## C 运行时编译（Phase 1–5）

### CMake 构建

```cmake
# CMakeLists.txt 顶层结构
cmake_minimum_required(VERSION 3.16)
project(Camelino VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 平台选择: host / arduino
set(CAMELINO_PLATFORM "host" CACHE STRING "Target platform")

# 核心源文件
set(CORE_SOURCES
    src/core/value.c src/core/memory.c src/core/error.c
    src/core/vm.c src/core/bytecode.c)

# FFI 源文件
set(FFI_SOURCES src/ffi/ffi.c src/ffi/primitives.c)

# REPL 源文件 (Phase 6.5)
set(REPL_SOURCES src/repl/repl.c src/repl/line_editor.c src/repl/history.c)

# 绑定层 (Phase 4)
set(BINDINGS_SOURCES
    src/bindings/gpio.c src/bindings/serial.c
    src/bindings/analog.c src/bindings/time.c src/bindings/error.c)
```

### 测试构建（cat+compile 模式）

大多数测试使用单步 `cat + gcc` 编译模式（规避 MinGW linker 兼容问题）：

```cmake
add_custom_target(test_vm_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_vm.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        > test_vm_combined.c
    COMMAND gcc test_vm_combined.c
        -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -o test_vm.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_vm_combined.c
    COMMENT "Building test_vm"
)
add_test(NAME test_vm COMMAND test_vm.exe)
```

### 14 个 C 单元测试

| # | 测试 | Phase | 源文件 | 子测试 |
|---|------|-------|--------|--------|
| 1 | test_value | 1.1 | value.c | 7 |
| 2 | test_opcodes | 1.3 | opcodes.h | 10 |
| 3 | test_memory | 1.2 | memory/value/error | 8 |
| 4 | test_vm | 1.4 | vm/memory/value/error | 17 |
| 5 | test_bytecode | 1.5 | bytecode/vm/memory/value/error | 7 |
| 6 | test_embed | 1.6 | bytecode/vm/memory/value/error | — |
| 7 | test_calls | 2.1 | vm/memory/value/error | 3 |
| 8 | test_exn | 2.3 | vm/memory/value/error | 2 |
| 9 | test_globals | 2.4 | vm/memory/value/error | — |
| 10 | test_gc | 3.1 | vm/memory/value/error | 3 |
| 11 | test_ffi | 4.1 | vm/memory/value/error/ffi | 2 |
| 12 | test_phase4 | 4.4 | vm/memory/value/error/ffi | — |
| 13 | test_phase5 | 5.1 | vm/memory/value/error/ffi | 6 |
| 14 | test_gc_roots | 4.2 | gc_roots/memory/value/error | — |

---

## OCaml 工具编译（Phase 6）

### camelino-embed（.cmo → .camel 转换器）

```bash
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed.exe main.ml

# 使用
./camelino-embed.exe input.cmo -o output.camel
```

### camelino-check（静态兼容性分析）

```bash
cd tools/camelino-check
dune build main.exe

# 使用
./_build/default/main.exe input.cmo --heap 192k --word-size 32 --platform arduino

# 或通过 wrapper
../../tools/camelino-check.sh input.cmo --heap 192k --platform arduino
```

### camelino-repl（交互式 REPL）

```bash
cd tools/camelino-repl
dune build main.exe

# Local 模式
echo '2 + 3' | ./_build/default/main.exe --local
```

---

## 主机模拟器编译（Phase 6.2）

```bash
# 手动单步编译 (cat + gcc)
echo '#define _DARWIN_C_SOURCE' > /tmp/prefix.h
cat /tmp/prefix.h \
    src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c \
    src/core/bytecode.c src/core/gc_roots.c \
    src/ffi/ffi.c src/ffi/primitives.c \
    platform/host/hal_adapter.c platform/host/port_init.c \
    > /tmp/host_comb.c

gcc /tmp/host_comb.c \
    -Isrc/core -Iplatform/host -Isrc -Isrc/hal -Isrc/ffi \
    -std=gnu99 -DCAMELINO_HAS_FFI \
    -o build/camelino-host

# 使用
./build/camelino-host --version
./build/camelino-host --acc-only file.camel           # 差分测试模式
./build/camelino-host --trace file.camel              # 逐指令 trace
./build/camelino-host --gc-stats file.camel           # GC 统计
./build/camelino-host --heap-stats file.camel         # 堆用量统计
```

---

## Arduino / PlatformIO 编译（Phase 6.4/6.6）

### Arduino IDE

1. 安装 Arduino-Pico core (Earle Philhower)
2. 打开 `examples/Blink/Blink.ino`
3. 将 `bytecode.h` 放在同目录
4. Board: Raspberry Pi Pico 2 → Upload

### PlatformIO

```bash
# platformio.ini 已配置三个环境
pio run -e pico2         # RP2350
pio run -e pico          # RP2040
pio run -e esp32dev      # ESP32

# 一键烧录
pio run -e pico2 -t upload
```

### camelino deploy（一键命令）

```bash
chmod +x tools/camelino-deploy.sh
./tools/camelino-deploy.sh blink.ml
./tools/camelino-deploy.sh --check --board rpipico2 --port /dev/ttyACM0 blink.ml
```

---

## 差分测试套件（Phase 6.3）

### 快速（3 项）

```bash
bash tools/test_suite/run_diff.sh
```

### 全量（41 项，分 8 类）

```bash
bash tools/test_suite/run_diff_full.sh
```

| 类别 | 数量 | 示例 |
|------|------|------|
| 算术 | 17 | add/sub/mul/div/mod/neg/and/or/xor/lsl/lsr + const |
| 比较 | 8 | eq/neq/lt/le/gt/ge |
| 分支 | 3 | branch/branchif/branchifnot |
| 栈 | 2 | assign/acc0 |
| 全局 | 2 | setglobal/getglobal |
| 异常 | 2 | raise caught/uncaught |
| FFI | 1 | C_CALL1 identity |
| P1指令 | 6 | boolnot/isint/ultint/beq/bneq |

### 添加新差分测试

在 `tools/test_suite/diff_tests.c` 中：

```c
static void pN_new_category(void) {
    printf("\n── Phase N: New Category ────────────────────────\n");
    {uint8_t c[]={CONST3,PUSH,CONSTINT,5,0,0,0,ADDINT,STOP};
     T("my_test_name",c,sizeof(c),8);}
}
```

在 `main()` 中添加 `pN_new_category();` 调用。

---

## C Stub 集成（Phase 6.9）

```bash
# 列出 C stub 中的 caml_* 符号
./tools/camelino-stub.sh scan tools/test_stub.c

# 生成 Arduino wrapper header
./tools/camelino-stub.sh wrap tools/test_stub.c -o stubs.h

# 完整 bundle: bytecode + C stubs
./tools/camelino-stub.sh embed tools/test_stub.c --bytecode file.camel -o bundle.h
```

---

## ci.yml（GitHub Actions，Phase 6.6）

```yaml
# .github/workflows/ci.yml
# 3 job 矩阵:
#   host-tests:    ubuntu + macos → cmake build + ctest(14) + diff(41)
#   ocaml-tools:   ubuntu → camelino-embed + check + repl
#   syntax-check:  ubuntu → 26 文件语法检查
```

---

## 已注册的 C Primitive（Phase 3–5）

| 类别 | Primitive | 状态 |
|------|-----------|------|
| String | caml_string_length/get/create/blit | ✅ 真实实现 |
| Bytes | caml_bytes_length/get/create/blit | ✅ 真实实现 |
| Format | caml_format_int, caml_int_of_string | ⚠️ 桩 |
| Format | caml_format_float, caml_float_of_string | ⚠️ 桩 |
| 通道 I/O | caml_ml_output_char/input_char/output/flush | ✅ 真实实现 |
| 通道 I/O | caml_ml_open_descriptor_out/in | ✅ 真实实现 |
| Random | caml_random_init, caml_random_int | ✅ LCG |
| Hash | caml_hash_variant, caml_hash_univ | ✅ FNV-1a |
| Lazy | caml_update_dummy, caml_lazy_make_forward | ⚠️ 桩 |
| GPIO | caml_camellino_digital_write/read/pin | ✅ HAL |
| Time | caml_camellino_delay_ms/millis | ✅ HAL |
| Serial | caml_camellino_serial_write | ✅ HAL |
| Exception | caml_failwith/invalid_argument/array_bound_error | ⚠️ 桩 |
| Sys | caml_sys_exit/install_signal_handler | ⚠️ 桩 |

---

## 平台说明

| 平台 | cmake + ctest | 差分测试 | OCaml 工具 | Arduino 交叉编译 |
|------|--------------|---------|-----------|----------------|
| Linux x86-64 | ✓ 14/14 | ✓ 41/41 | ✓ | — |
| macOS ARM64 | ✓ 14/14 | ✓ 41/41 | ✓ | — |
| Windows MinGW | 语法替代 | ✓ | ✓ | — |
| Arduino RP2350 | — | — | — | ✓ (需 hardware) |
