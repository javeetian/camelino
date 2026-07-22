# Camelino 验证步骤（Phase 1–5）

## 前置条件

| 工具 | 版本 | 安装 |
|------|------|------|
| GCC | ≥ 12 | `pacman -S mingw-w64-ucrt-x86_64-gcc` (MSYS2) |
| CMake | ≥ 3.16 | `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| Ninja | 任意 | `pacman -S mingw-w64-ucrt-x86_64-ninja` |
| OCaml | 4.14.x | `opam switch create 4.14.2` |

## 快速全量验证

```bash
# Linux/macOS: 一键全过
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build
ctest --test-dir build --output-on-failure
cd tools/test_suite && bash run_diff.sh

# MinGW (Windows): cmake 部分 target 会因 linker 失败，
# 以下 3 个可执行文件稳定可用，其余用语法验证
./build/test_value.exe
./build/test_opcodes.exe
./build/test_memory.exe
cd tools/test_suite && bash run_diff.sh
```

---

## Phase 1：值表示 + ZAM 虚拟机

### 1.1 值表示 — test_value

**验证内容：** `Val_long`/`Long_val`/`Is_long`/`Is_block`/`Field`/`Hd_val`/header。
对齐 OCaml 官方 `runtime/caml/mlvalues.h`。

**运行：**
```bash
./build/test_value.exe
```

**预期输出：**
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

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_value.c src/core/value.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_value syntax OK"
```

---

### 1.2 内存分配器 — test_memory

**验证内容：** 静态堆 bump 分配、值栈 push/pop、全局变量 set/get。

**运行：**
```bash
./build/test_memory.exe
```

**预期输出：**
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

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_memory.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_memory syntax OK"
```

---

### 1.3 ZAM 指令定义 — test_opcodes

**验证内容：** 149 条 ZAM 指令枚举值与 OCaml 4.14 `runtime/caml/instruct.h` 对齐。

**运行：**
```bash
./build/test_opcodes.exe
```

**预期输出：**
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

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_opcodes.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_opcodes syntax OK"
```

---

### 1.4 ZAM 解释器核心 — test_vm

**验证内容：** 手工构造字节码，喂给 `caml_interpret()`，比对 acc 值。

**运行（Linux/macOS）：**
```bash
./build/test_vm.exe
```

**预期输出：**
```
=== Camelino VM Tests (P0 Instructions) ===
  test_const_stop                    OK
  test_add_2_3                       OK    ← CONST3,PUSH,CONST2,ADDINT,STOP → acc=5
  test_constint_100                  OK
  test_sub_10_3                      OK
  test_mul_6_7                       OK
  test_div_6_2                       OK
  test_mod_10_3                      OK
  test_cmp_gt                        OK
  test_cmp_eq                        OK
  test_cmp_neq                       OK
  test_branch                        OK
  test_branchif_true                 OK
  test_branchifnot_false             OK
  test_stop_halt                     OK
  test_stack_arith                   OK
  test_negint                        OK
  test_bitwise                       OK
=== All 17 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_vm.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_vm syntax OK"
```

---

### 1.5 字节码加载器 — test_bytecode

**验证内容：** `.camel` 格式解析 + CRC32 校验 + VM 集成。

**运行（Linux/macOS）：**
```bash
./build/test_bytecode.exe
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_bytecode.c src/core/bytecode.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_bytecode syntax OK"
```

---

### 1.6 camelino-embed 工具 + 端到端 — test_embed

**验证内容：** ocamlc → camelino-embed → VM 完整链路。

**运行（Linux/macOS）：**
```bash
./build/test_embed.exe
```

**手动复现：**
```bash
# 编译 camelino-embed
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed.exe main.ml
cd ../..

# 编译 OCaml → .cmo → .camel → VM
echo 'let _ = 2 + 3' > /tmp/add23.ml
ocamlc -c -o /tmp/add23.cmo /tmp/add23.ml
tools/camelino-embed/camelino-embed.exe /tmp/add23.cmo -o /tmp/add23.camel
xxd /tmp/add23.camel | grep 00000020
# 预期看到: 657f 033a 3900 0000 008f ...
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_embed.c src/core/bytecode.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_embed syntax OK"
```

---

### 1.7 差分测试

**验证内容：** 对 `ocamlrun` 差分比对。

**运行：**
```bash
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

---

## Phase 2：函数 + 控制流

### 2.1-2.2 闭包 + 函数调用 — test_calls

**验证内容：** CLOSURE(4.14 closinfo), APPLY1/APPLY2, RETURN, GRAB, PUSH_RETADDR。

**运行（Linux/macOS）：**
```bash
./build/test_calls.exe
```

**预期输出：**
```
=== Phase 2.x: Closure & Call Tests ===
  test_apply1                        OK    ← f(x)=x+1, call f(5) → 6
  test_apply1_99                     OK    ← f(x)=x+1, call f(99) → 100
  test_apply2_offsets                OK    ← f(x,y)=y+1, call f(3,4) → 5
=== All 3 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_calls.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_calls syntax OK"
```

---

### 2.3 异常处理 — test_exn

**验证内容：** PUSHTRAP, POPTRAP, RAISE, RAISE_NOTRACE。

**运行（Linux/macOS）：**
```bash
./build/test_exn.exe
```

**预期输出：**
```
=== Phase 2.3: Exception Tests ===
  test_raise_basic                   OK    ← PUSHTRAP → CONST3 → RAISE → handler sees acc=3
  test_raise_uncaught                OK    ← uncaught RAISE → VM halts
=== All 2 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_exn.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_exn syntax OK"
```

---

### 2.4 全局变量 — test_globals

**验证内容：** SETGLOBAL, GETGLOBAL, GETGLOBALFIELD, PUSHGETGLOBAL, PUSHGETGLOBALFIELD。

**运行（Linux/macOS）：**
```bash
./build/test_globals.exe
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_globals.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_globals syntax OK"
```

---

### 2.5 + 2.6 启动流程 + 差分测试

验证方式同上——Phase 1 差分测试持续通过即表示启动流程正确。

---

### 2.7 内部 primitive

`src/ffi/primitives.c` 中 7 个桩（failwith, invalid_argument 等）：

```bash
gcc -fsyntax-only src/ffi/primitives.c -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 && echo "primitives syntax OK"
```

---

## Phase 3：GC + 堆块

### 3.1 Mark-Sweep GC — test_gc

**验证内容：** GC mark/sweep、free list 分配、堆耗尽自动触发。

**运行（Linux/macOS）：**
```bash
./build/test_gc.exe
```

**预期输出：**
```
=== Phase 3.1: GC Tests ===
  test_gc_basic                      OK
  test_gc_alloc_free                 OK
  test_gc_preserves_live             OK
=== All 3 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_gc.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_gc syntax OK"
```

---

## Phase 4：FFI + HAL

### 4.1 Primitive 调度 — test_ffi

**验证内容：** C_CALL1 通过索引调用 C 函数（prim_identity, prim_add1）。

**运行（Linux/macOS）：**
```bash
./build/test_ffi.exe
```

**预期输出：**
```
=== Phase 4.1: FFI Tests ===
  test_ffi_identity                  OK    ← C_CALL1(identity, 42) → 42
  test_ffi_add1                      OK    ← C_CALL1(add1, 5) → 6
=== All 2 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_ffi.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c src/ffi/ffi.c -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 && echo "test_ffi syntax OK"
```

---

### 4.2 GC 根保护 — test_gc_roots

**验证内容：** CAMLparam1/CAMLlocal1/CAMLreturn 保护 GC 根。

**运行（Linux/macOS）：**
```bash
./build/test_gc_roots.exe
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_gc_roots.c src/core/gc_roots.c src/core/memory.c src/core/value.c src/core/error.c -Isrc/core -Iplatform/host -std=gnu99 && echo "test_gc_roots syntax OK"
```

---

### 4.4-4.10 综合 — test_phase4

**验证内容：** HAL primitive (digital_write) + C_CALL2 dispatch。

**运行（Linux/macOS）：**
```bash
./build/test_phase4.exe
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_phase4.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c src/ffi/ffi.c -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 && echo "test_phase4 syntax OK"
```

---

## Phase 5：完整 ZAM + stdlib

### 5.1 P1 指令 + 5.3 stdlib — test_phase5

**验证内容：** BEQ/BNEQ, BOOLNOT, OFFSETREF, ISINT, create_string, hash。

**运行（Linux/macOS）：**
```bash
./build/test_phase5.exe
```

**预期输出：**
```
=== Phase 5: P1 Instruction Tests ===
  test_beq_true                      OK
  test_bneq_true                     OK
  test_boolnot                       OK
  test_isint                         OK
  test_string_create_len             OK    ← create_string(3) → string_length → 3
  test_hash                          OK    ← hash_variant(42) → non-zero hash
=== All 6 tests passed! ===
```

**MinGW 替代：**
```bash
gcc -fsyntax-only test/test_phase5.c src/core/vm.c src/core/memory.c src/core/value.c src/core/error.c src/ffi/ffi.c -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 && echo "test_phase5 syntax OK"
```

---

### 5.3 String/Bytes

**验证内容：** 4 个 primitive（create/get/length/blit）。

手动验证 bytecode：
```
create_string(3) → string_length 返回 3
create_string(5) → string_get(s, 0) 返回首字节
```

---

### 5.3 Hashtbl

**验证内容：** `caml_hash_variant` (FNV-1a 简化版)。

```bash
# 编译 bytecode: CONSTINT(42), PUSH, C_CALL1(26), STOP
# 预期 acc ≠ 0（hash 值非零）
```

---

### 5.3 Random

**验证内容：** `caml_random_int`（LCG）。

---

### 5.3 Buffer / Format

纯 OCaml 模块，依赖的底层 primitive 已全部实现：
- Buffer → `caml_create_bytes` / `caml_blit_bytes` / `caml_bytes_length` / `caml_bytes_get`
- Format → `caml_ml_output_char` / `caml_ml_flush` / `caml_format_int`

验证方式：用 `ocamlc` 编译 `buffer.ml` → `camelino-embed` → VM 执行。

---

### 5.6 通道 I/O

**验证内容：** `caml_ml_output_char(0, 'A')` 输出到 stdout。

```bash
# 编译 bytecode: CONSTINT(65)=A, PUSH, CONSTINT(0)=channel, PUSH, C_CALL2(17), STOP
# 预期 stdout 打印 'A'
```

---

## 语法批量验证（MinGW 替代方案）

所有 23 个源文件的一次性语法检查：

```bash
for f in src/core/*.c src/ffi/*.c test/test_*.c; do
  printf "%-45s" "${f##*/}: "
  gcc -fsyntax-only $f -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 2>&1 && echo "OK" || echo "FAIL"
done
```

预期输出：
```
bytecode.c:                                  OK
error.c:                                     OK
gc_roots.c:                                  OK
memory.c:                                    OK
value.c:                                     OK
vm.c:                                        OK
ffi.c:                                       OK
primitives.c:                                OK
test_bytecode.c:                             OK
test_calls.c:                                OK
test_embed.c:                                OK
test_exn.c:                                  OK
test_ffi.c:                                  OK
test_gc.c:                                   OK
test_gc_roots.c:                             OK
test_globals.c:                              OK
test_memory.c:                               OK
test_opcodes.c:                              OK
test_phase4.c:                               OK
test_phase5.c:                               OK
test_value.c:                                OK
test_vm.c:                                   OK
=== 22 passed, 0 failed ===
```

---

## 平台说明

| 平台 | cmake + ctest | 差分测试 | 语法验证 |
|------|--------------|---------|---------|
| Linux x86-64 | ✓ 全过 | ✓ | ✓ |
| macOS ARM64 | ✓ 全过 | ✓ | ✓ |
| Windows MinGW GCC 14.2 | 仅前 3 个（linker bug） | ✓ | ✓ |
| Windows MSVC | 待验证 | — | — |
