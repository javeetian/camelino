# Camelino 编译与测试指南

## 前置条件

| 工具 | 版本要求 | 安装方式 |
|------|---------|---------|
| GCC | ≥ 12 | `pacman -S mingw-w64-ucrt-x86_64-gcc`（MSYS2） |
| CMake | ≥ 3.16 | `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| Ninja | 任意 | `pacman -S mingw-w64-ucrt-x86_64-ninja` |
| OCaml | 4.14.x | `opam switch create 4.14.2` |

## 快速开始

```bash
# 1. 配置（仅需一次）
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc

# 2. 编译
cmake --build build

# 3. 运行所有测试
ctest --test-dir build --output-on-failure
```

预期输出：
```
100% tests passed, 0 tests failed out of 4
```

## 单独运行某个测试

```bash
# 值表示测试（7 个子测试）
./build/test_value.exe

# 指令集测试（10 个子测试，验证 opcode 值与 OCaml 4.14 对齐）
./build/test_opcodes.exe

# 内存分配器测试（8 个子测试）
./build/test_memory.exe

# 虚拟机测试（17 个子测试，P0 指令解释执行）
./build/test_vm.exe
```

## 测试清单

### test_value — 值表示（Phase 1.1）

验证 `Val_long`/`Long_val`/`Is_long`/`Is_block`/`Field`/`Hd_val`/header 操作。
与 OCaml 官方 `runtime/caml/mlvalues.h` 对齐：整数低位=1，指针低位=0。

```bash
./build/test_value.exe
```

预期输出：
```
=== Camelino Value Module Tests (OCaml-official tag convention) ===

  test_integer_roundtrip             OK
  test_tag_discrimination            OK
  test_pointer_and_header            OK
  test_block_access                  OK
  test_constants_and_converters      OK
  test_tag_constants                 OK
  test_large_integers                OK

=== All 7 tests passed! ===
```

### test_opcodes — 指令集（Phase 1.3）

验证 149 条 ZAM 指令的枚举值与 OCaml 4.14 `runtime/caml/instruct.h` 一致。

```bash
./build/test_opcodes.exe
```

### test_memory — 内存分配器（Phase 1.2）

验证静态堆 bump 分配、值栈 push/pop、全局变量 set/get。

```bash
./build/test_memory.exe
```

### test_vm — 虚拟机（Phase 1.4）

手工构造字节码，喂给 `caml_interpret()` 执行，验证 P0 指令计算结果。

| 测试 | 字节码 | 期望 |
|------|--------|------|
| test_const_stop | `CONST1, STOP` | acc = 1 |
| test_add_2_3 | `CONST3, PUSH, CONST2, ADDINT, STOP` | acc = 5 |
| test_constint_100 | `CONSTINT(100), STOP` | acc = 100 |
| test_sub_10_3 | `CONST3, PUSH, CONSTINT(10), SUBINT, STOP` | acc = 7 |
| test_mul_6_7 | `CONSTINT(7), PUSH, CONSTINT(6), MULINT, STOP` | acc = 42 |
| test_div_6_2 | `CONST2, PUSH, CONSTINT(6), DIVINT, STOP` | acc = 3 |
| test_mod_10_3 | `CONST3, PUSH, CONSTINT(10), MODINT, STOP` | acc = 1 |
| test_cmp_gt | `CONSTINT(5), PUSH, CONST3, GTINT, STOP` | acc = true |
| test_cmp_eq | `CONST3, PUSH, CONST3, EQ, STOP` | acc = true |
| test_cmp_neq | `CONSTINT(5), PUSH, CONST3, NEQ, STOP` | acc = true |
| test_branch | `CONST1, BRANCH(offset=5), CONST3, STOP, STOP` | acc = 1 |
| test_branchif_true | `CONST1, BRANCHIF(offset=5), CONST3, STOP, STOP` | acc = 1 |
| test_branchifnot_false | `CONST0, BRANCHIFNOT(offset=5), CONST3, STOP, STOP` | acc = 0 |
| test_stop_halt | `CONST1, STOP, CONST3` | halt=true, acc=1 |
| test_stack_arith | `CONST3, PUSH, CONST2, ADDINT, STOP` | acc = 5 |
| test_negint | `CONSTINT(42), NEGINT, STOP` | acc = -42 |
| test_bitwise | AND/OR/XOR 各一个测试 | 2, 7, 5 |

```bash
./build/test_vm.exe
```

## 如何增加新测试

1. 在 `test/` 下创建 `test_xxx.c`
2. 在 `CMakeLists.txt` 中注册目标

对于简单的测试（不依赖 memory.c）：
```cmake
add_executable(test_xxx test/test_xxx.c)
target_include_directories(test_xxx PRIVATE ${PLATFORM_DIR} ${CAMELINO_ROOT}/src/core)
add_test(NAME test_xxx COMMAND test_xxx)
```

对于需要 memory/value/error 的测试，使用单步编译模式（规避 MinGW linker 问题）：
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

## 平台说明

- **当前开发平台**：主机模拟器（`platform/host/`），64 位小端，硬 FPU
- **交叉编译到 RP2350**：Phase 4 启用，换 `platform/arduino/` 适配器
- **已知问题**：MinGW GCC 14.2 + Ninja 多 `.o` 链接时 collect2 有兼容问题，已改用单步 `cat + gcc` 编译模式绕开
