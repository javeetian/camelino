# Camelino 编译与测试指南

## 前置条件

| 工具 | 版本要求 | 安装方式 |
|------|---------|---------|
| GCC | ≥ 12 | `pacman -S mingw-w64-ucrt-x86_64-gcc`（MSYS2） |
| CMake | ≥ 3.16 | `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| Ninja | 任意 | `pacman -S mingw-w64-ucrt-x86_64-ninja` |
| OCaml | 4.14.x | `opam switch create 4.14.2` |

---

## 一键编译与测试

```bash
# 1. 配置（仅需一次）
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc

# 2. 编译所有目标
cmake --build build

# 3. 运行全部测试
ctest --test-dir build --output-on-failure
```

预期输出：
```
100% tests passed, 0 tests failed out of 6
```

---

## 测试清单（按 Phase 顺序）

### 1. test_value — 值表示（Phase 1.1）

**验证内容：** `Val_long`/`Long_val`/`Is_long`/`Is_block`/`Field`/`Hd_val`/header 操作。
对齐 OCaml 官方 `runtime/caml/mlvalues.h`：整数低位=1，指针低位=0。

**源文件：** `test/test_value.c` + `src/core/value.c`

**CMake 构建方式：**
```cmake
add_executable(test_value test/test_value.c src/core/value.c)
```

**运行：**
```bash
./build/test_value.exe
```

**预期输出（7 个子测试）：**
```
=== Camelino Value Module Tests ===
  test_integer_roundtrip             OK
  test_tag_discrimination            OK
  test_pointer_and_header            OK
  test_block_access                  OK
  test_constants_and_converters      OK
  test_tag_constants                 OK
  test_large_integers                OK
=== All 7 tests passed! ===
```

---

### 2. test_memory — 内存分配器（Phase 1.2）

**验证内容：** 静态堆 bump 分配、值栈 push/pop、全局变量 set/get。

**源文件：** `test/test_memory.c` + `src/core/value.c` + `src/core/memory.c` + `src/core/error.c`

**CMake 构建方式（单步 cat+compile，规避 MinGW linker）：**
```cmake
add_custom_target(test_memory_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_memory.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        > test_memory_combined.c
    COMMAND gcc test_memory_combined.c
        -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -o test_memory.exe
    ...
)
```

**运行：**
```bash
./build/test_memory.exe
```

**预期输出（8 个子测试）：**
```
=== Camelino Memory Allocator Tests ===
  test_init                          OK
  test_alloc_single                  OK
  test_alloc_read_write              OK
  test_alloc_multiple                OK
  test_alloc_tags                    OK
  test_color_preserved               OK
  test_stack_basic                   OK
  test_globals_basic                 OK
=== All 8 tests passed! ===
```

---

### 3. test_opcodes — 指令集定义（Phase 1.3）

**验证内容：** 149 条 ZAM 指令枚举值与 OCaml 4.14 `runtime/caml/instruct.h` 完全对齐。

**源文件：** `test/test_opcodes.c`（仅含 `opcodes.h`，无其他依赖）

**CMake 构建方式：**
```cmake
add_executable(test_opcodes test/test_opcodes.c)
target_include_directories(test_opcodes PRIVATE ${PLATFORM_DIR} ${CAMELINO_ROOT}/src/core)
```

**运行：**
```bash
./build/test_opcodes.exe
```

**预期输出（10 个子测试）：**
```
=== Camelino Opcode Tests (vs OCaml 4.14 instruct.h) ===
  test_instruction_count             OK
  test_p0_core_values                OK
  test_function_call_ops             OK
  test_block_ops                     OK
  test_branch_ops                    OK
  test_exception_ops                 OK
  test_ffi_ops                       OK
  test_const_ops                     OK
  test_control_ops                   OK
  test_object_ops                    OK
=== All 10 tests passed! ===
```

---

### 4. test_vm — ZAM 虚拟机（Phase 1.4）

**验证内容：** P0 指令手工字节码 → `caml_interpret()` 执行 → 比对 acc 计算结果。

**源文件：** `test/test_vm.c` + `src/core/vm.c` + `src/core/memory.c` + `src/core/value.c` + `src/core/error.c`

**CMake 构建方式：** 同 test_memory，单步 cat+compile

**运行：**
```bash
./build/test_vm.exe
```

**测试覆盖表：**

| 子测试 | 手工字节码 | 期望 acc |
|--------|-----------|---------|
| test_const_stop | `CONST1, STOP` | 1 |
| test_add_2_3 | `CONST3, PUSH, CONST2, ADDINT, STOP` | 5 |
| test_constint_100 | `CONSTINT(100), STOP` | 100 |
| test_sub_10_3 | `CONST3, PUSH, CONSTINT(10), SUBINT, STOP` | 7 |
| test_mul_6_7 | `CONSTINT(7), PUSH, CONSTINT(6), MULINT, STOP` | 42 |
| test_div_6_2 | `CONST2, PUSH, CONSTINT(6), DIVINT, STOP` | 3 |
| test_mod_10_3 | `CONST3, PUSH, CONSTINT(10), MODINT, STOP` | 1 |
| test_cmp_gt | `CONSTINT(5), PUSH, CONST3, GTINT, STOP` | true |
| test_cmp_eq | `CONST3, PUSH, CONST3, EQ, STOP` | true |
| test_cmp_neq | `CONSTINT(5), PUSH, CONST3, NEQ, STOP` | true |
| test_branch | `CONST1, BRANCH(offset=5), CONST3, STOP, STOP` | 1 |
| test_branchif_true | `CONST1, BRANCHIF(offset=5), CONST3, STOP, STOP` | 1 |
| test_branchifnot_false | `CONST0, BRANCHIFNOT(offset=5), CONST3, STOP, STOP` | 0 |
| test_stop_halt | `CONST1, STOP, CONST3` | 1 + halted |
| test_stack_arith | `CONST3, PUSH, CONST2, ADDINT, STOP` | 5 |
| test_negint | `CONSTINT(42), NEGINT, STOP` | -42 |
| test_bitwise | AND/OR/XOR 各一个 | 2, 7, 5 |

---

### 5. test_bytecode — 字节码加载器（Phase 1.5）

**验证内容：** `.camel` 格式解析 + CRC32 校验 + VM 集成执行。

**源文件：** `test/test_bytecode.c` + `src/core/bytecode.c` + `src/core/vm.c` + `src/core/memory.c` + `src/core/value.c` + `src/core/error.c`

**CMake 构建方式：** 同 test_memory，单步 cat+compile

**运行：**
```bash
./build/test_bytecode.exe
```

**预期输出（7 个子测试）：**
```
=== Camelino Bytecode Loader Tests ===
  test_load_simple                   OK    ← 完整链路：构造.camel → load → VM执行 → acc=5
  test_bad_magic                     OK    ← magic 错误 → CAML_BC_ERR_BAD_MAGIC
  test_wrong_word_size               OK    ← word_size 不匹配 → CAML_BC_ERR_WORD_SIZE
  test_bad_crc                       OK    ← CRC 错误 → CAML_BC_ERR_CHECKSUM
  test_too_small                     OK    ← 数据过短 → CAML_BC_ERR_TOO_SMALL
  test_entry_offset_default          OK    ← entry_offset 默认值 = 0
  test_reload                        OK    ← 重复加载覆盖旧数据
=== All 7 tests passed! ===
```

---

### 6. test_embed — 端到端流水线（Phase 1.6 验收）

**验证内容：** 完整工具链 ocamlc → camelino-embed → .camel → VM。

**源文件：** `test/test_embed.c` + `src/core/bytecode.c` + `src/core/vm.c` + `src/core/memory.c` + `src/core/value.c` + `src/core/error.c`

**CMake 构建方式：** 同 test_memory，单步 cat+compile

**运行：**
```bash
./build/test_embed.exe
```

**手动复现完整流水线：**
```bash
# 1. 编译 OCaml 源码 → .cmo
echo 'let _ = 2 + 3' > /tmp/add23.ml
ocamlc -c -o /tmp/add23.cmo /tmp/add23.ml

# 2. 编译 camelino-embed 工具
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg \
  -package compiler-libs.bytecomp,compiler-libs.common \
  -o camelino-embed.exe main.ml
cd ../..

# 3. .cmo → .camel
tools/camelino-embed/camelino-embed.exe /tmp/add23.cmo -o /tmp/add23.camel

# 4. 查看 .camel 二进制内容
xxd /tmp/add23.camel

# 5. 自动化验证
./build/test_embed.exe
```

---

### 7. run_diff.sh — 差分测试（Phase 1.7 验收）

**验证内容：** 对 `ocamlrun` 做差分比对。每个 `.ml` 用例经 `ocamlc` 编译 → `camelino-embed` 提取字节码 → 内联 runner 执行 → 比对 `.expect` 期望值。

**源文件：** `tools/test_suite/run_diff.sh` + `tools/test_suite/case*.ml` + `tools/test_suite/case*.expect`

**编译与运行：**
```bash
# 编译 camelino-embed（首次）
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed.exe main.ml
cd ../..

# 运行差分测试
cd tools/test_suite && bash run_diff.sh
```

**预期输出：**
```
=== Camelino Differential Tests (Phase 1) ===

  PASS case01_add (2+3) (expected=5)
  PASS case02_sub (10-3) (expected=7)
  PASS case03_mul (6*7) (expected=42)

=== Results: 3 passed, 0 failed ===
```

**手动复现单条用例：**
```bash
cd tools/test_suite

# 1. 编译 .ml → .cmo
ocamlc -c -o _t.cmo case01_add.ml

# 2. .cmo → .camel（查看字节码）
../camelino-embed/camelino-embed.exe _t.cmo -o _t.camel

# 3. 查看字节码 hex
xxd _t.camel | grep 00000020
# 输出: 657f 033a 3900 0000 008f  → CONST2, OFFSETINT(3), ATOM0, SETGLOBAL(0), STOP

# 4. 期望值
cat case01_add.expect   # 输出: 5
```

---

## 单独编译和运行某个测试

```bash
# 只编译（所有目标）
cmake --build build

# 只运行某个测试
ctest --test-dir build -R test_vm --output-on-failure

# 或者直接运行可执行文件
./build/test_vm.exe
```

## 添加新测试

1. 在 `test/` 下创建 `test_xxx.c`
2. 在 `CMakeLists.txt` 中注册目标

**简单测试（不依赖 memory/vm/error）：**
```cmake
add_executable(test_xxx test/test_xxx.c)
target_include_directories(test_xxx PRIVATE ${PLATFORM_DIR} ${CAMELINO_ROOT}/src/core)
add_test(NAME test_xxx COMMAND test_xxx)
```

**需要 memory/value/error/vm 的测试（单步 cat+compile）：**
```cmake
add_custom_target(test_xxx_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_xxx.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        > test_xxx_combined.c
    COMMAND gcc test_xxx_combined.c
        -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -o test_xxx.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_xxx_combined.c
    DEPENDS ${CAMELINO_ROOT}/test/test_xxx.c ...
    COMMENT "Building test_xxx"
)
add_test(NAME test_xxx COMMAND test_xxx.exe)
```

---

## 平台说明

- **当前开发平台**：主机模拟器（`platform/host/`），64 位小端，硬 FPU
- **交叉编译到 RP2350**：Phase 4 启用，换 `platform/arduino/` 适配器
- **已知问题**：MinGW GCC 14.2 + Ninja 多 `.o` 链接时 collect2 有兼容问题，已改用单步 `cat + gcc` 编译模式绕开
