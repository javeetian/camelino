# platform/host — 主机模拟器构建说明

## 用途

主机模拟器用于在 PC 上开发、调试和差分测试 Camelino 虚拟机，无需 RP2350 硬件。
所有 Phase 1–3 的开发工作在此平台上完成。

## 前置条件

| 工具 | 安装方式 |
|------|---------|
| GCC ≥ 12 | MSYS2: `pacman -S mingw-w64-ucrt-x86_64-gcc` |
| CMake ≥ 3.16 | MSYS2: `pacman -S mingw-w64-ucrt-x86_64-cmake` |
| Ninja | MSYS2: `pacman -S mingw-w64-ucrt-x86_64-ninja` |
| OCaml 4.14.x | `opam switch create 4.14.2` |

## 平台配置

`platform/host/platform_config.h`:
```
CAMELINO_WORD_SIZE = 64    (64 位小端)
CAMELINO_BIG_ENDIAN = 0
CAMELINO_HAS_FPU = 1
HEAP_SIZE = 16 MB
STACK_SIZE = 65536
MAX_GLOBALS = 65536
```

## 构建

```bash
cd camelino
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build
```

产物（Linux/macOS 全部可用；MinGW 仅前 3 个稳定）：
- `build/test_value.exe` — 值表示测试 ✓
- `build/test_opcodes.exe` — 指令集测试 ✓
- `build/test_memory.exe` — 分配器测试 ✓
- `build/test_vm.exe` — 虚拟机测试（MinGW 可能失败）
- `build/test_calls.exe` — 函数调用测试
- `build/test_exn.exe` — 异常测试
- `build/test_globals.exe` — 全局变量测试
- `build/test_gc.exe` — GC 测试
- `build/test_ffi.exe` — FFI 测试
- `build/test_phase4.exe` — Phase4 集成测试
- `build/test_phase5.exe` — P1 指令测试
- `build/test_bytecode.exe` — 字节码加载器测试
- `build/test_embed.exe` — 端到端流水线测试

## 运行测试

```bash
ctest --test-dir build --output-on-failure   # Linux/macOS: 全部 13 个
./build/test_vm.exe                           # 单独运行某个
```

## MinGW 替代验证

本机 MinGW GCC 14.2 的 collect2 link 大文件时会损坏 exe。用语法验证替代：
```bash
for f in src/core/*.c src/ffi/*.c test/test_*.c; do
  echo -n "$f: "
  gcc -fsyntax-only $f -Isrc/core -Iplatform/host -Isrc/ffi -std=gnu99 2>&1 && echo "OK" || echo "FAIL"
done
```
```

## 差分测试

```bash
cd tools/camelino-embed
ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml
cd ../..
cd tools/test_suite && bash run_diff.sh
```

## 平台适配器说明

`platform/host/hal_adapter.c` 把 HAL 接口映射到 POSIX/libc：
- `hal_uart_write` → `fwrite(stdout)`
- `hal_time_millis` → `GetTickCount64()` (Win) / `clock_gettime()` (Unix)
- `hal_delay_ms` → `Sleep()` (Win) / `usleep()` (Unix)
- `hal_gpio_write` → `fprintf(stderr, log)`

## 已知问题

- MinGW GCC 14.2 的 collect2 在多 `.o` 链接时有问题，
  已改用单文件 `cat + gcc` 编译模式绕开
- camelino-host 交互式可执行文件构建不稳定，
  差分测试改用内联 C runner 脚本
