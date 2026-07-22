# Camelino 贡献指南

欢迎贡献！无论是修复 bug、添加功能、改进文档，还是移植新平台。

## 开发环境

```bash
# macOS
brew install cmake ninja gcc

# Linux
sudo apt-get install -y gcc cmake ninja-build

# OCaml 4.14.x
opam switch create 4.14.2
eval $(opam env)
```

## 项目架构

```
src/core/     — ZAM 虚拟机 (平台无关 C99)
src/ffi/      — C primitive 注册与调度
src/bindings/ — CAMLprim 包装层 (OCaml value ↔ C 类型)
src/hal/      — HAL 接口声明 (平台无关)
src/repl/     — 设备端 REPL
platform/     — 平台适配器 (每个平台一个目录)
tools/        — PC 端工具 (OCaml)
test/         — C 单元测试
lib/          — OCaml 端 FFI 库
```

## 编译 & 测试

```bash
# 编译 (主机模拟器)
cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
cmake --build build

# 运行全部测试
ctest --test-dir build --output-on-failure           # 14 项 C 单元测试
bash tools/test_suite/run_diff_full.sh                 # 41 项差分测试

# 构建 OCaml 工具
cd tools/camelino-embed && ocamlfind ocamlc -linkpkg \
  -package compiler-libs.common -o camelino-embed main.ml
cd tools/camelino-check && dune build
cd tools/camelino-repl  && dune build
```

## 代码风格

- **C**: C99 (`-std=gnu99`)，4 空格缩进，`/** */` 文档注释
- **OCaml**: 标准 OCaml 风格，`(* *)` 注释
- 所有公共 API 必须有文档注释
- 多字节整数使用 `CAML_READ_*` 宏（不裸指针解引用）

## 测试规范

- **新增 ZAM 指令** → 在 `diff_tests.c` 中添加至少 1 个差分测试
- **新增 C 函数** → 在 `test/` 中添加单元测试
- **修改 VM 核心** → 运行全量回归 (ctest + diff tests)
- 确保 14 项 C 测试 + 41 项差分测试全部通过

## 提交信息规范

```
phase: 简短描述

详细说明（可选）
```

示例：
```
phase5: implement FORINTLOOP instruction

Adds the FORINTLOOP ZAM instruction for OCaml for-loop support.
Includes diff test case.
```

## 添加新平台

参见 [PORTING.md](PORTING.md)

## 发布流程

1. 所有测试通过
2. 更新 Design.md 版本号
3. 更新 Work.md 勾选完成的任务
4. `git tag vX.Y.Z`
5. 发布 OPAM 包

## 许可证

MIT — 参见 [LICENSE](LICENSE)
