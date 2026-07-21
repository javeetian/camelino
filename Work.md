# Camelino 工作计划

**基于：** [Design.md](Design.md) v0.4.0
**生成日期：** 2026-07-21
**总工期估算：** 20 周

---

## 工作总览

```
Phase 0  →  项目基础设施搭建（第 1 周）
Phase 1  →  值表示 + 最小 ZAM（第 1–3 周）      ← 主机模拟器（camelino-host）
Phase 2  →  函数 + 控制流（第 4–6 周）          ← 主机模拟器
Phase 3  →  GC + 堆块（第 7–9 周）              ← 主机模拟器
Phase 4  →  FFI + HAL + 首个平台适配器（第 10–11 周）← 交叉编译到 RP2350
Phase 5  →  完整 ZAM + stdlib（第 12–16 周）
Phase 6  →  生态工具 + 多平台移植（第 17–20 周）
```

> **开发策略：Phase 1–3 全程在主机模拟器上完成。** 虚拟机核心是平台无关 C99，主机编译秒级反馈，差分测试对比 `ocamlrun` 保证字节码兼容。Phase 4 才交叉编译到 RP2350（只需换 HAL 适配器，零 VM 改动）。

---

## Phase 0：项目基础设施搭建（在 Phase 1 之前或与之并行）

### 任务 0.1：项目目录结构与构建系统

**目标：** 搭建可编译、可测试的骨架。

- [ ] **0.1.1** 创建完整目录树
  - `src/core/`、`src/ffi/`、`src/repl/`、`src/bindings/`、`src/hal/`
  - `platform/arduino/`、`platform/picosdk/`、`platform/espidf/`、`platform/zephyr/`、`platform/baremetal/`、`platform/host/`
  - `tools/camelino-embed/`、`tools/camelino-check/`、`tools/test_suite/`
  - `test/`（设备端 C 单元测试）、`examples/`（已部分存在）
  - `lib/camelino/`（已部分存在）
- [ ] **0.1.2** 搭建构建系统
  - Arduino 构建：`library.properties`（已存在，需完善）、`CMakeLists.txt`（PlatformIO 兼容）
  - 主机模拟器构建：CMake + GCC/Clang，编译 `platform/host/` 目标
  - 设备端测试构建：CMake + 交叉编译工具链（`arm-none-eabi-gcc` for RP2350）
  - 工具链构建：OCaml dune 项目（`tools/camelino-embed/`、`tools/camelino-check/`）
- [ ] **0.1.3** OCaml 开发环境搭建
  - 安装 `opam`，创建 switch（`ocaml-variants.4.14.1+options,ocaml-option-32bit` 用于 32 位 target）
  - 安装 `ocamlfind`、`dune`、`ocamlclean`（评估兼容性）
  - 确认 `ocamlc -version` = 4.14.1
- [ ] **0.1.4** CI 流水线骨架
  - GitHub Actions：主机端 C 编译 + 单元测试 + OCaml 工具编译 + 差分测试
  - 代码风格检查：`.clang-format`（C99）、`ocamlformat`（OCaml）

### 任务 0.2：基础头文件与跨平台宏

**目标：** 实现 §1.4 的编译期参数化宏，所有后续代码依赖这些宏。

- [ ] **0.2.1** `src/core/camelino_config.h` — 平台配置聚合
  - 由各 `platform/<name>/platform_config.h` 提供下列宏，本文件统一 include 路径：
    - `CAMELINO_WORD_SIZE`（32 或 64）
    - `CAMELINO_BIG_ENDIAN`（0 或 1）
    - `CAMELINO_HAS_FPU`（0 或 1）
    - `HEAP_SIZE`、`STACK_SIZE`、`MAX_GLOBALS`
  - 编译期静态断言：`sizeof(void*) * 8 == CAMELINO_WORD_SIZE`
- [ ] **0.2.2** `src/core/byteorder.h` — 字节序显式读写宏
  - `CAML_READ_UINT16(p)`、`CAML_READ_UINT32(p)`、`CAML_READ_UINT64(p)`
  - `CAML_READ_INT16(p)`、`CAML_READ_INT32(p)`、`CAML_READ_INT64(p)`
  - `CAML_WRITE_UINT16(p,v)`、`CAML_WRITE_UINT32(p,v)`、`CAML_WRITE_UINT64(p,v)`
  - 所有宏内部根据 `CAMELINO_BIG_ENDIAN` 选择字节序
  - `CAML_READ_DOUBLE(p)`：`memcpy` 到 `double` 临时变量（不假设对齐，§3.4.3）
  - **禁止裸指针解引用多字节类型**（铁律）
- [ ] **0.2.3** `src/core/platform.h` — 平台类型定义
  - `typedef intptr_t value;`（§5）
  - `typedef value header_t;`
  - `typedef uint32_t mlsize_t;`（32 位 target 够用，64 位可改为 `size_t`）
  - `typedef uint8_t tag_t;`、`typedef uint8_t color_t;`
- [ ] **0.2.4** 主机平台骨架 `platform/host/`
  - `platform_config.h`：64 位小端、硬 FPU、大堆
  - `port_init.c`：`main()` 入口
  - `hal_adapter.c`：桩实现（大部分用 libc/POSIX 模拟）
  - 目标：Phase 1 即可在 PC 上编译运行 ZAM，做差分测试（§10.4）

### 任务 0.3：错误处理与调试基础设施

**目标：** 统一的 fatal 错误输出 + 开发期调试开关。

- [ ] **0.3.1** 致命错误统一 API（`src/core/error.h`）
  - `caml_fatal_error(const char* msg)`：UART 输出错误串 + 寄存器 dump + 停机
  - `caml_fatal_errorv(const char* fmt, ...)`：格式化版本
  - 触发场景：栈溢出 / OOM / 字节码校验失败 / 未捕获异常（§6.4）
- [ ] **0.3.2** 错误码枚举 `caml_err_t`
  - `CAMELINO_OK` / `CAMELINO_ERR_STACK_OVERFLOW` / `CAMELINO_ERR_OUT_OF_MEMORY` / `CAMELINO_ERR_BYTECODE_INVALID` / `CAMELINO_ERR_UNCAUGHT_EXN` / `CAMELINO_ERR_INTERNAL`
  - 与 `hal_err_t` 区分命名空间
- [ ] **0.3.3** 断言宏 `CAMELINO_ASSERT(cond)`
  - Debug 构建：断言失败 → UART trace + halt
  - Release 构建：编译为空（零开销）
- [ ] **0.3.4** 调试 trace 开关
  - `#ifdef CAMELINO_DEBUG_TRACE` 编译选项
  - 每条指令可选输出：`pc` / `acc` / `sp` / 指令名 / 操作数
  - GC 统计：每轮 GC 输出回收/存活块数、耗时
  - 堆用量：当前 heap 使用量 / 总量

---

## Phase 1：值表示 + 最小 ZAM 虚拟机（第 1–3 周）

**前置依赖：** Phase 0（0.2 字节序宏 + 0.3 错误框架）必须先行完成。1.1→1.2→1.4 严格串行（值表示→分配器→解释器）。1.6（embed）可与 1.1–1.4 并行开发。

**开发策略：全程在主机模拟器（`camelino-host`）上开发验证。** 主机编译+运行秒级反馈，免除烧录-调试慢循环。虚拟机核心代码（`vm.c`、`memory.c`、`value.c`）是平台无关的 C99——在 host 上跑通过后，Phase 4 交叉编译到 RP2350 只需换 HAL 适配器。差分测试对 `ocamlrun` 验证正确性。

```
开发循环：
  写代码 → ninja → ./build/camelino-host → 差分测试对比 ocamlrun → 通过 ✓
  （全程 < 10 秒，不需要 RP2350 硬件）

Phase 4 交叉编译到 RP2350 只是一次性移植步骤。
```

**核心交付物：ZAM 虚拟机引擎**（`src/core/vm.c/h`，平台无关 C99）。

```
          ┌──────────────────────────────────┐
          │   ZAM 虚拟机（跑在 MCU 上）         │
          │                                  │
          │   caml_interpret()               │
          │     for (;;) {                   │
          │       opcode = *pc++;             │
          │       switch (opcode) {           │
          │         case ADDINT: ...          │
          │       }                           │
          │       if (yield) return;          │
          │     }                             │
          │                                  │
          │   输入：camelino_code[]（Flash）    │
          │   状态：pc/acc/sp/env/trap         │
          │   内存：静态堆 + 静态栈（无 malloc） │
          └──────────────────────────────────┘
```

虚拟机通过 `caml_interpret()` 循环逐条执行 ZAM 指令——**fetch → decode → execute → yield/continue**。Phase 1 只实现 P0 指令子集（约 40 条），够跑 `2+3`；Phase 2 加入函数/闭包/异常（P0 余下的调用相关指令）；Phase 5 补全全部 149 条。

**目标：** 解释器能执行 `2 + 3`，差分测试通过。

### 任务 1.1：值表示系统（`src/core/value.h` / `value.c`）

**目标：** 实现 §5 的 tagged integer 表示，与 OCaml 官方 `ocaml/values.h` 对齐。

- [ ] **1.1.1** Tagged integer 基础宏
  - `Is_long(x)`、`Is_block(x)`（低位约定：整数=1，指针=0）
  - `Val_long(x)`、`Long_val(x)`、`Val_int(x)`、`Int_val(x)`
  - `Val_true`、`Val_false`、`Val_unit`、`Val_empty`
  - `Bool_val(x)`、`Val_bool(b)`
- [ ] **1.1.2** 堆块操作宏
  - `Hd_val(b)`：header 在 `block[-1]`
  - `Field(x, i)`：字段访问
  - `Hd_with_wosize(w)`、`Make_header(wosize, tag, color)`（§5.0）
  - `Wosize_hd(hd)`、`Tag_hd(hd)`、`Color_hd(hd)`
- [ ] **1.1.3** 堆块标签常量（与 OCaml 4.14 对齐，§5.1）
  - `Closure_tag = 247`、`String_tag = 252`、`Double_tag = 253`
  - `Double_array_tag = 254`、`Custom_tag = 255`
  - `Lazy_tag = 245`、`Object_tag = 248`、`Infix_tag = 249`
  - `Forward_tag = 250`、`Abstract_tag = 251`
- [ ] **1.1.4** 单元测试 `test/test_value.c`
  - 覆盖：`Val_long`/`Long_val` 往返、header 打包/解包、`Field` 读写
  - 覆盖：32 位 / 64 位字长下的行为差异
  - 断言：`Is_long(Val_long(42))` 为真、`Is_block` 为假

### 任务 1.2：Bump Allocator（无 GC）

**目标：** 实现 §6.2 的静态内存池 + 简单 bump 分配器（GC 在 Phase 3 加入）。

- [ ] **1.2.1** 内存池初始化（`src/core/memory.h` / `memory.c`）
  - `caml_memory_init()`：初始化 `heap`、`stack`、`globals` 三个静态数组
  - `caml_alloc(wosize, tag)`：bump 分配，从 `heap` 底部向上增长
  - `caml_alloc_small(wosize, tag)`：小对象优化（预留）
- [ ] **1.2.2** 栈管理
  - `caml_stack` 静态数组，`sp` 指针
  - 栈溢出检测：`PUSH`/`PUSHRETADDR`/`PUSHTRAP`/`APPLY` 处检查 guard
- [ ] **1.2.3** 全局变量区
  - `caml_globals` 静态数组
  - `GETGLOBAL`/`SETGLOBAL` 按槽位号索引

### 任务 1.3：ZAM 指令定义（`src/core/opcodes.h`）

**目标：** 所有 ZAM 指令的枚举常量，与 OCaml 4.14 `instruct.h` 对齐。

- [ ] **1.3.1** 完整指令枚举
  - 以 `ocaml/runtime/instruct.h` 为权威来源
  - 约 149 条指令，按 P0/P1/P2 标注优先级（注释标记）
- [ ] **1.3.2** 指令操作数解码辅助宏
  - `GET_S8(pc)`、`GET_U8(pc)`、`GET_S16(pc)`、`GET_U16(pc)`
  - `GET_INT32(pc)`（经 `CAML_READ_INT32`）
  - 各指令操作数位数/符号以 `instruct.ml` 为准

### 任务 1.4：ZAM 解释器核心（`src/core/vm.h` / `vm.c`）

**目标：** 实现 P0 指令子集的解释循环。

- [ ] **1.4.1** VM 状态结构体
  - `pc`、`acc`、`sp`、`env`、`trap`、`extra_args`
  - `global_data`、`prim_table`
  - `yield_counter`（§9.4 调度）
- [ ] **1.4.2** P0 指令实现（约 40 条，§4.2）
  - **常量：** `ACC0..ACC7`、`ACC`、`CONST0..CONST3`、`CONSTINT`
  - **栈：** `PUSH`、`PUSHACC`（含 `PUSHACC0..7`）、`POP`、`ASSIGN`
  - **整数运算：** `ADDINT`、`SUBINT`、`MULINT`、`DIVINT`、`MODINT`
    - ⚠️ `DIVINT`/`MODINT` 操作数最低位 = `raise_on_overflow` 标志，漏判导致除零语义错误
    - ⚠️ `ADDINT`/`SUBINT`/`MULINT` 操作数最低位 = `raise_on_overflow` 标志
  - **比较：** `EQ`、`NEQ`、`LTINT`、`LEINT`、`GTINT`、`GEINT`
    - ⚠️ 比较指令操作数高位编码 `Comparison_xxx`，不可当纯整数
  - **分支：** `BRANCH`、`BRANCHIF`、`BRANCHIFNOT`
  - **函数调用：** `PUSHRETADDR`、`APPLY`（1-3 参数）、`APPTERM`（尾调用）、`RETURN`、`RESTART`、`GRAB`
  - **闭包：** `CLOSURE`、`CLOSUREREC`
  - **全局：** `GETGLOBAL`、`SETGLOBAL`、`GETGLOBALFIELD`
  - **堆块：** `MAKEBLOCK`（1-3）、`GETFIELD`、`SETFIELD`、`GETVECTITEM`、`SETVECTITEM`
  - **FFI 桩：** `C_CALL1`..`C_CALL5`、`C_CALLN`（调度框架，primitive 表为空）
- [ ] **1.4.3** 解释器主循环（`caml_interpret()`）
  - `for(;;)` + `switch(*pc++)`
  - 指令计数 → yield flag 检查（§9.4，Phase 2 正式启用）
- [ ] **1.4.4** 操作数读取规范
  - 所有多字节操作数经 `CAML_READ_*` 宏（§1.4），禁止裸指针解引用
  - 指令流按 `pc++` / `pc += sizeof(operand)` 推进

### 任务 1.5：字节码加载器（`src/core/bytecode.h` / `bytecode.c`）

**目标：** 解析 `.camel` 格式，校验后加载。

- [ ] **1.5.1** `.camel` 格式解析（§4.4 格式 2）
  - 读取 magic `CAMELINO01`
  - 读取 `format_ver`（uint16）、`ocaml_ver`（string）、`word_size`（uint8）、`big_endian`（uint8）、`fpu_mode`（uint8）
  - 读取 `stdlib_crc`（uint32）
  - 读取 code_size + code[]、data_size + data[]
  - 读取 num_prims + prim_names[]
  - 读取 entry_offset、checksum
- [ ] **1.5.2** 二进制兼容校验（§3.4.4）
  - magic 校验
  - `word_size` 与 `sizeof(value)` 校验（不匹配 → 拒绝）
  - `big_endian` 与目标校验（见 §3.4.2）
  - OCaml 版本主版本号校验（不一致 → 拒绝）
  - `stdlib_crc` 与运行时 stdlib 子集 CRC 校验（不匹配 → 拒绝）
  - **CRC32 checksum 校验**（CRC-32/ISO-HDLC，覆盖 code[] + data[]；传输/Flash 损坏检测）
- [ ] **1.5.3** 全局数据初始化（§3.2.2）
  - 按拓扑序执行各 CU 初始化代码
  - `SETGLOBAL` 填充 `global_data[]`

### 任务 1.6：`camelino-embed` 工具原型（`tools/camelino-embed/`）

**目标：** PC 端工具：解析 `.cmo` → 重定位 → 字长重写 → 输出 `bytecode.h`。

- [ ] **1.6.1** `.cmo` 解析
  - Marshal 格式反序列化（`cu_code`、`cu_codesize`、`cu_primitives`、`cu_reloc_table`、`cu_required_globals`、`cu_global`）
  - magic 校验、stdlib CRC 校验（§3.4.4）
- [ ] **1.6.2** `.cma` 解析（可选，为多模块预留）
  - 归档头 + N 个 `.cmo` 体 + 单元索引列表
- [ ] **1.6.3** 拓扑排序
  - 按 `cu_required_globals` 依赖关系排序 CU
  - 检测循环依赖 → 报错
- [ ] **1.6.4** 重定位（RELOC）应用
  - `RELOC_GETGLOBAL` → 最终 `global_data` 槽位号
  - `RELOC_SETGLOBAL` → 最终 `global_data` 槽位号
  - `RELOC_GETGLOBALFIELD` → 槽位号 + 字段
  - 代码合并后**跳转偏移重算**：多个 CU 的字节码段拼接成连续 `code[]` 后，所有 `BRANCH` 族指令的相对偏移须加上段基址增量
- [ ] **1.6.5** 字长重写（方案 B，§3.4.1.1）
  - [ ] **1.6.5.1** 同字长直通模式（64→64 或 32→32）：校验 + 零拷贝直出（fast path）
  - [ ] **1.6.5.2** header 重打包：`Wosize` 位宽在 22 ↔ 54 位间转换，`Color`/`Tag` 位段不变（§5.0）
  - [ ] **1.6.5.3** tagged integer 截断检查：扫描全局数据所有 tagged int，超 target 位宽 → 编译期报错
  - [ ] **1.6.5.4** `host_word_offset → target_byte_offset` 映射表：按字节数守恒重算每个 word 的 new 偏移
  - [ ] **1.6.5.5** 指针/偏移按映射表重写：所有 `Is_block` 字段的指针值换算为 target 偏移
  - [ ] **1.6.5.6** 字符串 padding 重算（§5.3）：`sizeof(value)` 变化后重新填充末尾字节
  - [ ] **1.6.5.7** 浮点块容器缩窄/扩宽：`double` 大小不变，但容器 word 数变化（tag 253/254）
  - [ ] **1.6.5.8** 端到端验证：64→32 重写后的字节码在 target 差分通过（§10.4）
- [ ] **1.6.6** 输出生成（`bytecode.h` 格式，§3.2.4）
  - 重定位后指令流（目标序）
  - 全局数据初值（目标 word/序）
  - primitive 名表、入口符号
  - **CRC-32 checksum**（CRC-32/ISO-HDLC，覆盖 code[] + data[]）

### 任务 1.7：差分测试框架（`tools/test_suite/`）

**目标：** 搭建 §10.4 的差分测试基础设施。

- [ ] **1.7.1** 测试脚本骨架
  - 对每个测试 `.ml` 文件 → `ocamlc` 编译 → `ocamlrun` 执行（参考输出）
  - 对每个测试 `.ml` 文件 → `camelino-embed` → 主机模拟器执行（实际输出）
  - diff 参考输出 vs 实际输出
- [ ] **1.7.2** Phase 1 测试用例
  - `2 + 3` → 期望 5
  - 基本整数四则运算
  - 条件分支（`if/then/else`）

### 任务 1.8：内部 C primitive 追踪矩阵（跨 Phase）

**目标：** 建立 OCaml runtime 所需的全部 C primitive 清单，按阶段标注实现时机，防止遗漏。

以下 primitive 虽然由 `C_CALLN` 调度，但它们是 OCaml 语言语义的一部分，不是"可选的硬件绑定"：

| Primitive | 作用 | 实现阶段 | 备注 |
|-----------|------|----------|------|
| `caml_equal`、`caml_notequal` | 结构化 `=` / `<>` | Phase 1 | 整数/布尔直接用 ZAM `EQ`；结构化类型走 C_CALLN |
| `caml_compare`、`caml_int_compare` | 多态 `<` / `>` / `<=` / `>=` | Phase 1 | 仅实现整数/布尔；结构化比较推迟到 Phase 3 |
| `caml_failwith` | `failwith "msg"` | Phase 2 | 分配 `Failure` 异常块 + raise |
| `caml_invalid_argument` | `invalid_arg "msg"` | Phase 2 | 分配 `Invalid_argument` 异常块 + raise |
| `caml_array_bound_error` | 数组越界异常 | Phase 2 | — |
| `caml_raise_with_arg` | 带参数的标准异常 raise | Phase 2 | 如 `Not_found` / `Sys_error` |
| `caml_raise_with_string` | 带 string 参数的 raise | Phase 2 | — |
| `caml_string_length`、`caml_string_get` | 字符串字面量/索引 | Phase 3 | §5.3 布局相依 |
| `caml_create_string`、`caml_blit_string` | 字符串分配/复制 | Phase 3 | — |
| `caml_string_equal`、`caml_string_notequal` | 字符串比较 | Phase 3 | — |
| `caml_make_vect`、`caml_array_get`、`caml_array_set` | Array 模块 | Phase 3 | `MAKEBLOCK` + 读写 |
| `caml_obj_tag`、`caml_obj_size`、`caml_obj_field` | 反射（Obj 模块）| Phase 3 | — |
| `caml_register_global_root` | C 代码注册 GC 根 | Phase 3 | C stub 跨分配持有 OCaml 值的刚需 |
| `caml_remove_global_root` | 注销 GC 根 | Phase 3 | — |
| `caml_format_int`、`caml_int_of_string` | 整数 ↔ 字符串 | Phase 4 | — |
| `caml_format_float`、`caml_float_of_string` | 浮点 ↔ 字符串 | Phase 4 | 软 FPU 下注意精度 |
| `caml_ml_input`、`caml_ml_output` | 通道 I/O 基础 | Phase 5 | **Printf 不直接调 UART，而是经通道→hal_uart_write** |
| `caml_ml_flush` | 刷新输出通道 | Phase 5 | — |
| `caml_ml_open_descriptor_in/out` | 打开通道描述符 | Phase 5 | 映射 UART 端口到 OCaml channel |
| `caml_update_dummy` | 强制 lazy 值 | Phase 5 | `Lazy_tag`(245) 相关 |
| `caml_lazy_make_forward` | 创建懒求值转发指针 | Phase 5 | `Forward_tag`(250) 相关 |
| `caml_install_signal_handler` | 桩实现 | Phase 2 | 设备端无信号，空返回 |
| `caml_sys_exit` | 桩实现 | Phase 2 | 设置 `halted = true`（§9.4 STOP 语义） |

> **各阶段验收前必须确认本阶段所需的上述 primitive 均已实现，未实现的改为显式桩（返回 `Val_unit` 或抛出 `Failure`），防止链接期缺失。**

### 任务 1.9：Phase 1 内部 C primitive（桩）

**目标：** 确保 Phase 1 能编译链接。

- [ ] **1.9.1** `caml_equal` / `caml_notequal` / `caml_compare`（Phase 1 桩）
  - Phase 1 仅有整数运算，结构化 `=` / `<` 走 ZAM `EQ` / `LTINT` 等指令
  - 桩实现：若参数是整数则比较，否则调用 `caml_fatal_error("unimplemented primitive")`
  - 完整实现推迟到 Phase 3（3.7.5）
- [ ] **1.9.2** 其余 1.8 矩阵中标为 Phase 2+ 的 primitive
  - 注册为桩（返回 `Val_unit` 或 `caml_fatal_error`），防止 `C_CALLN` 查表时崩溃

### Phase 1 验收标准

**所有验收在主机模拟器（`camelino-host`）上完成，不依赖 RP2350 硬件。**

- [ ] `ocamlc` 编译的 `let _ = 2 + 3` → `camelino-host` 输出 `5`
- [ ] 差分测试：`camelino-host` 输出与 `ocamlrun` 一致（diff 零差异）
- [ ] 所有 `test_value` 单元测试通过
- [ ] `camelino-embed` 工具能将 `2+3` 的 `.cmo` 生成正确的 `bytecode.h`，host 加载后差分通过

---

## Phase 2：函数 + 控制流（扩展虚拟机，第 4–6 周）

**前置依赖：** Phase 1 验收通过（P0 指令 + embed + 差分测试可用）。2.1→2.2→2.3 严格串行（调用帧→闭包→异常）。2.5（启动流程）可与 2.1–2.3 并行开发；2.6（测试）在各子任务完成后增量补充。

**核心交付物：** 虚拟机支持函数调用（APPLY/GRAB/RETURN）、闭包（CLOSURE/CLOSUREREC）、异常（PUSHTRAP/RAISE）。

**目标：** `ocamlc` 编译的 `let rec fib n = ...` 正常运行（含异常）。

### 任务 2.1：调用栈帧实现（§4.5.1）

**目标：** 返回帧布局与 `ocamlrun` 完全一致。

- [ ] **2.1.1** `PUSHRETADDR` / `RETURN` 帧
  - 3-slot 帧：`[saved_extra_args, saved_env, saved_pc(return)]`
  - `RETURN n`：弹出 n 个参数 + 3-slot 帧，恢复寄存器
- [ ] **2.1.2** `APPLY` 完整实现
  - 设置 `extra_args = 实际参数数 - 1`
  - 压入返回帧
  - 读 `closinfo` 获取 arity
  - 跳转到闭包 code pointer
- [ ] **2.1.3** `APPTERM` 尾调用优化
  - 丢弃当前帧，重用栈空间
  - 正确计算丢弃的 slot 数（含参数 + 返回帧 + 局部变量）
  - 否则栈无限增长
- [ ] **2.1.4** `RESTART` 多参数重组
  - 配合 `GRAB` 处理多余参数
  - 从栈上重新组织参数帧

### 任务 2.2：闭包表示（§5.2，4.14 含 closinfo）

**目标：** 正确解析 4.14 闭包布局。

- [ ] **2.2.1** 闭包块创建（`CLOSURE` / `CLOSUREREC`）
  - Tag 247 块分配
  - field 0：code pointer（字节码偏移）
  - field 1：closinfo（编码 arity + closure kind）
  - field 2..n：自由变量（`Start_env = 2`）
- [ ] **2.2.2** `closinfo` 解码
  - arity：`GRAB`/`APPLY` 据此判断参数是否足够
  - closure kind：`Pure`（可尾调用）/ `Default`
- [ ] **2.2.3** `GRAB` 机制完整实现
  - 参数刚好 → 执行函数体
  - 参数不足 → 创建部分应用闭包（currying）
  - 参数过多 → 执行后对返回值继续 `APPLY`
- [ ] **2.2.4** 环境访问（`ACC`/`PUSHACC` 索引语义）
  - 索引 0 → `env`（闭包自身）
  - 索引 ≥2 → 从 `env+1` 起的自由变量

### 任务 2.3：异常处理（§4.5.2）

**目标：** Trap 帧 + `RAISE` 栈展开完整实现。

- [ ] **2.3.1** `PUSHTRAP` / `POPTRAP` / `RAISE` / `RAISE_NOTRACE`
  - 4-slot trap 帧：`[handler_pc, saved_sp_offset, saved_env, saved_extra_args]`
  - `RAISE`：找到栈顶最近 trap 帧，回退 `sp`，恢复 `env`/`extra_args`
  - `trap` 寄存器串联 trap 帧链
- [ ] **2.3.2** Trap 帧链遍历
  - 栈展开遍历至匹配 handler
  - 无匹配 handler → 未捕获异常 → 设备端致命错误
- [ ] **2.3.3** 预定义异常值
  - `Match_failure`、`Division_by_zero`、`Overflow`、`Stack_overflow`、`Out_of_memory`
  - 异常值在全局数据中预分配

### 任务 2.4：全局变量初始化（拓扑序执行）

**目标：** 按拓扑序执行各 CU 初始化，填充 `global_data[]`。

- [ ] **2.4.1** `caml_init_globals()`（§9.1）
  - 按拓扑序（从 `camelino-embed` 输出中获取）
  - 逐个 CU 执行初始化代码
  - 每个 CU 结束后 `SETGLOBAL` 自己的模块值
- [ ] **2.4.2** `GETGLOBALFIELD` 完整实现
  - 槽位号 + 字段偏移 → 读取全局块内的子字段

### 任务 2.5：启动与执行流程（§9.1）

**目标：** 完整启动流程串联。

- [ ] **2.5.1** `caml_start()` 实现
  - `caml_memory_init()` → `caml_init_primitives()` → `caml_load_bytecode()` → `caml_init_globals()` → `caml_interpret()`
- [ ] **2.5.2** Arduino `setup()` / `loop()` 集成（`Camelino.cpp`）
  - `setup()`：调用 `caml_start()`
  - `loop()`：空（初始版本）
- [ ] **2.5.3** 主机模拟器入口（`platform/host/main.c`）
  - `main()` → `caml_start()`

### 任务 2.6：Phase 2 差分测试

- [ ] **2.6.1** 函数测试用例
  - `let rec fib n = if n < 2 then n else fib (n-1) + fib (n-2)` → 期望正确序列
  - 多参数函数：`let add3 a b c = a + b + c`
  - 部分应用（currying）：`let add = (+) 1`
  - 尾调用：深递归不栈溢出
  - 嵌套闭包：`let make_adder x = fun y -> x + y`
- [ ] **2.6.2** 异常测试用例
  - `try ... with` 基本捕获
  - 嵌套 try（多层 trap）
  - `raise` 不带 try → 预期致命错误
  - `Division_by_zero`（`DIVINT` 的 `raise_on_overflow` 标志）
- [ ] **2.6.3** 所有差分类比对 `ocamlrun` 输出

### 任务 2.7：Phase 2 内部 C primitive

**目标：** 实现本阶段 OCaml 语言语义所需的内部 primitive（在 §1.8 矩阵中标为 Phase 2 的条目）。

- [ ] **2.7.1** 异常相关
  - `caml_failwith`：分配 `Failure` 异常块 + `RAISE`
  - `caml_invalid_argument`：分配 `Invalid_argument` 异常块 + `RAISE`
  - `caml_array_bound_error`：标准越界异常
  - `caml_raise_with_arg`、`caml_raise_with_string`
- [ ] **2.7.2** 桩 primitive
  - `caml_install_signal_handler`：空返回
  - `caml_sys_exit`：设置 `halted = true`
- [ ] **2.7.3 primitive 注册**：以上全部 `caml_register_global_primitive` 注册入表（在 `caml_init_primitives` 中）

### Phase 2 验收标准

- [ ] `let rec fib n = ...` 正常计算，差分测试通过
- [ ] 异常抛出/捕获与 `ocamlrun` 行为一致
- [ ] 尾调用不栈溢出
- [ ] 部分应用闭包正常工作

---

## Phase 3：GC + 堆块（第 7–9 周）

**前置依赖：** Phase 2 验收通过（函数/闭包/异常完整）。3.1（GC）是目前最高风险项——如果 mark sweep 写错，已通过的所有差分测试会随机失败。建议先实现 GC 到主机模拟器上充分验证再交叉编译到设备端。

**目标：** GC 回收不再使用的块，长时间运行无内存泄漏。

### 任务 3.1：Mark-Sweep GC（§6.3）

**目标：** 实现 §5.0 header Color 位驱动的标记-清扫 GC。

- [ ] **3.1.1** GC 颜色常量 + 根集合枚举
  - Color 位共 2 bit，支持 4 色（§5.0）：
    - `CAML_WHITE = 0`：可回收（新分配默认色）
    - `CAML_GRAY = 1`：待扫描（mark 栈中）
    - `CAML_BLACK = 2`：已扫描存活
    - `CAML_BLUE = 3`：空闲块（free list，非存活，不参与 sweep）
  - 根集合：值栈 / 全局变量 / 寄存器（acc/env/trap 链）/ 闭包自由变量
- [ ] **3.1.2** Mark 阶段
  - 从根集合出发 DFS/BFS 遍历
  - 每访问一个块：`Color = CAML_BLACK`
  - 遇到 `Forward_tag`：跳过（GC 已移动）
  - 遇到 `Abstract_tag`：不扫描字段（不透明 C 数据）
  - 遇到 `Double_array_tag`：按 double 数组长度扫描（非 word 粒度）
- [ ] **3.1.3** Sweep 阶段
  - 遍历 heap 中所有块
  - `Color = CAML_WHITE` → 回收：标记为 `CAML_BLUE`，合并到 free list
  - `Color = CAML_BLACK` → 重置为 `CAML_WHITE`（为下一轮 GC 准备）
  - `Color = CAML_GRAY` → 断言失败（mark 阶段应全部消费）
  - `Color = CAML_BLUE` → 跳过（已在 free list）
- [ ] **3.1.4** free list 管理
  - Sweep 后重建 free list（按块大小分类或单一链表）
  - `caml_alloc` 优先从 free list 取，不足则 bump；bump 也不足则触发 GC
- [ ] **3.1.4** GC 触发条件
  - `caml_alloc` 时堆使用量超过阈值
  - GC 后 `caml_alloc` 仍失败 → 抛 `Out_of_memory`
- [ ] **3.1.5** GC 安全点（同 §9.4 yield 机制）
  - `C_CALL*` 返回后
  - 指令计数阈值到达后（与 yield 复用计数器）

### 任务 3.2：字符串支持（tag 252，§5.3）

**目标：** 字符串分配/读取/长度计算正确。

- [ ] **3.2.1** `caml_alloc_string(len)` + 访问宏
  - `Wosize = (len / sizeof(value)) + 1`（向上取整 word 边界）
  - 末尾字节编码 padding count（`1..sizeof(value)`）
  - `#define String_val(x) ((char*)(x))`：字符串指针直接指向块首字段
  - `#define Bytes_val(x) ((char*)(x))`：与字符串同布局
- [ ] **3.2.2** `caml_string_length(s)` / `caml_bytes_length(b)`
  - 读取块末尾 padding count → 反算实际字节数
  - 公式：`Wosize_hd * sizeof(value) − padding_last_byte`
- [ ] **3.2.3** 字符串比较/复制
  - `caml_string_equal`、`caml_string_copy`
  - GC 扫描时正确处理字符串块（按字节块扫描非 word）

### 任务 3.3：浮点支持（tag 253/254，§5.4–5.5）

**目标：** boxed double + 浮点数组 + 软 FPU 兼容。

- [ ] **3.3.1** `MAKEFLOAT` / `MAKEFLOATBLOCK`
  - Tag 253 块（单个 boxed double）
- [ ] **3.3.2** `GETFLOATFIELD` / `SETFLOATFIELD`
  - Tag 检查：tag==254 → 浮点路径
  - 浮点路径：`memcpy(&d, (char*)v + i * sizeof(double), sizeof(double))`
  - ⚠️ 不假设对齐（软 FPU 下 double 可能 4 字节对齐）
  - 软 FPU：编译器链接 `-mfloat-abi=soft`
- [ ] **3.3.3** 浮点数组 GC 扫描
  - Tag 254：按 `Wosize * sizeof(value)` 字节连续扫描（非 word 扫描）
  - 浮点数组内部是 raw double，不包含 OCaml value 指针

### 任务 3.4：其他堆块类型

- [ ] **3.4.1** `Custom_tag`（255）基础支持
  - vtable 指针（finalizer/比较/哈希）
  - GC 不扫描字段，由 vtable 决定
- [ ] **3.4.2** `Infix_tag`（249）支持
  - 内嵌闭包：多闭包共享同一个堆块
- [ ] **3.4.3** `Object_tag`（248）基础支持
  - 方法调用 `GETMETHOD`、`GETPUBMET`、`GETDYNMET`（P2 完整实现）

### 任务 3.5：stdlib 基础模块（Phase 2-3）

**目标：** 提供设备端可用的 stdlib 子集字节码。

- [ ] **3.5.1** `Pervasives` / `Stdlib` 子集
  - 整数比较、布尔运算、引用（`ref` / `:=` / `!`）
  - 基本类型转换（`string_of_int` 等——依赖 `caml_string_of_int` C primitive）
- [ ] **3.5.2** `List` 模块
  - 纯函数链表操作，无 IO 依赖
- [ ] **3.5.3** `Array` 模块
  - `MAKEBLOCK` 创建的通用数组
  - `GETVECTITEM` / `SETVECTITEM` 实现完成即自然支持

### 任务 3.6：Phase 3 差分测试

- [ ] **3.6.1** GC 测试
  - 长时间分配+丢弃循环 → 内存不耗尽
  - 强制定点 GC → 存活对象未被回收
  - 循环引用不可达 → 被正确回收
- [ ] **3.6.2** 字符串测试
  - 字符串长度/比较/复制
  - 与 `ocamlrun` 输出一致
- [ ] **3.6.3** 浮点测试
  - 基本浮点四则运算
  - 浮点数组读写
  - 软 FPU（交叉编译）vs 硬 FPU（host）双端验证

### 任务 3.7：Phase 3 内部 C primitive

**目标：** 实现本阶段字符串/数组/反射相关 primitive（§1.8 矩阵）。

- [ ] **3.7.1** 字符串 primitive
  - `caml_string_length`、`caml_string_get`、`caml_create_string`
  - `caml_blit_string`、`caml_string_equal`、`caml_string_notequal`
- [ ] **3.7.2** 数组 primitive
  - `caml_make_vect`、`caml_array_get`、`caml_array_set`
- [ ] **3.7.3** 反射 primitive
  - `caml_obj_tag`、`caml_obj_size`、`caml_obj_field`、`caml_obj_set_field`
  - `caml_obj_block`（分配指定 tag 的块）
- [ ] **3.7.4** GC 根注册
  - `caml_register_global_root`、`caml_remove_global_root`
  - 实现：`global_root_table[]` + 引用计数/链表
- [ ] **3.7.5** 比较 primitive 补全
  - `caml_compare`：结构化多态比较（先比 tag，再比 size，再递归字段；浮点 NaN 处理）
  - `caml_equal`：结构化递归相等比较

### Phase 3 验收标准

- [ ] 长时间运行（≥1 小时）无内存泄漏
- [ ] GC 正确回收不可达对象
- [ ] 字符串/浮点操作与 `ocamlrun` 一致
- [ ] stdlib 的 List.map / Array.init 工作正常

---

## Phase 4：FFI + HAL + 首个平台适配器（第 10–11 周）

**前置依赖：** Phase 3 验收通过（GC 稳定）。4.1（primitive dispatch）依赖 1.8 内部 primitive 表；4.3（HAL）可与 4.1/4.2 并行开发；4.4（bindings）严格依赖 4.3 HAL 接口定义完毕；4.7（Arduino 适配器）依赖 4.4。

**目标：** OCaml 代码 `digital_write 25 1` 点灯。Camelino 从"解释器"变成"嵌入式运行时"。

### 任务 4.1：Primitive 注册与调度（§7.2）

**目标：** `C_CALL*` 指令能调用注册的 C 函数。

- [ ] **4.1.1** Primitive 表结构（`src/ffi/ffi.h` / `ffi.c`）
  - `typedef value (*caml_primitive)(value arg1, ...);`
  - Hash 表或数组索引（以 `camelino-embed` 输出的顺序为准）
- [ ] **4.1.2** `caml_register_global_primitive(name, func)`
  - 注册内置 primitive
  - 与 `bytecode.h` 中的 `prim_names[]` 对
- [ ] **4.1.3** `caml_init_primitives()`（§9.1）
  - 加载 `bytecode.h` 中的 `prim_names[]`
  - 每个 primitive 必须有对应 C 实现；缺失 → 链接期报错
- [ ] **4.1.4** `C_CALL1`..`C_CALL5`、`C_CALLN` 调度
  - 参数传递：value → C 类型（`Long_val` / `Bool_val` / `String_val`）
  - 返回值：C 类型 → value（`Val_long` / `Val_bool` / `caml_copy_string`）
  - `CAMLparam`/`CAMLlocal`/`CAMLreturn` GC 根保护宏

### 任务 4.2：FFI 包装宏（GC 根保护）

**目标：** C primitive 中正确保护 GC 根。

- [ ] **4.2.1** `CAMLparam` / `CAMLlocal` / `CAMLreturn` 宏
  - 在 C 函数栈上声明 GC 根数组
  - `CAMLparam0`..`CAMLparam5`、`CAMLlocal0`..`CAMLlocal5`
  - `CAMLxparam` 用于额外参数
- [ ] **4.2.2** `CAMLreturn` + `CAMLreturn0`
  - 从 GC 根数组注销后返回

### 任务 4.3：HAL 接口定义（`src/hal/`，§7.6）

**目标：** 定义平台无关的硬件抽象接口，所有 bindings 只调 HAL。

- [ ] **4.3.1** `hal/gpio.h` — GPIO 接口
  - `hal_gpio_init(pin, mode)`、`hal_gpio_write(pin, value)`
  - `hal_gpio_read(pin)`、`hal_gpio_toggle(pin)`
  - `hal_gpio_mode_t` 枚举（INPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT/OUTPUT_OD）
- [ ] **4.3.2** `hal/uart.h` — 串口接口
  - `hal_uart_init(port, cfg)`、`hal_uart_write(port, data, len)`
  - `hal_uart_read(port, buf, len)`（非阻塞）、`hal_uart_flush(port)`
- [ ] **4.3.3** `hal/time.h` — 时序接口
  - `hal_time_millis()`、`hal_time_micros()`
  - `hal_delay_ms(ms)`、`hal_delay_us(us)`
- [ ] **4.3.4** `hal/analog.h` — 模拟接口
  - `hal_adc_init(pin)`、`hal_adc_read(pin)`
  - `hal_pwm_init(pin, freq)`、`hal_pwm_write(pin, duty)`
- [ ] **4.3.5** `hal/flash.h` — Flash 存储接口
  - `hal_flash_read(offset, buf, len)`
  - `hal_flash_write(offset, buf, len)`
  - `hal_flash_erase(offset, len)`
- [ ] **4.3.6** `hal/interrupt.h` — 中断 flag 接口
  - `hal_gpio_attach_isr(pin, edge, flag)`
  - `hal_gpio_detach_isr(pin)`
  - `hal_isr_flag_take(flag)`：原子取走并清零
- [ ] **4.3.7** `hal/cap.h` — 能力探测
  - `hal_cap_supported(cap)`：运行时查询平台能力
  - `hal_cap_id_t` 枚举（`HAL_CAP_UART`、`HAL_CAP_FLASH`、`HAL_CAP_ADC`...）
- [ ] **4.3.8** 统一错误码 `hal_err_t`
  - `HAL_OK` / `HAL_ERR_INVAL` / `HAL_ERR_TIMEOUT` / `HAL_ERR_UNSUPPORTED`...

### 任务 4.4：Bindings 层（`src/bindings/`，§7.1）

**目标：** CAMLprim 包装层：只做 value ↔ C 转换 + 调 HAL。

- [ ] **4.4.1** `bindings/gpio.c` — GPIO bindings
  - `caml_camellino_digital_write(pin, value)` → `hal_gpio_write()`
  - `caml_camellino_digital_read(pin)` → `hal_gpio_read()`
  - `caml_camellino_pin_mode(pin, mode)` → `hal_gpio_init()`
- [ ] **4.4.2** `bindings/serial.c` — 串口 bindings
  - `caml_camellino_serial_begin(port, baud)` → `hal_uart_init()`
  - `caml_camellino_serial_write(port, str)` → `hal_uart_write()`
  - `caml_camellino_serial_read(port)` → `hal_uart_read()`
- [ ] **4.4.3** `bindings/time.c` — 时序 bindings
  - `caml_camellino_delay_ms(ms)` → `hal_delay_ms()`
  - `caml_camellino_millis()` → `hal_time_millis()`
- [ ] **4.4.4** `bindings/analog.c` — 模拟 bindings
  - `caml_camellino_analog_read(pin)` → `hal_adc_read()`
  - `caml_camellino_analog_write(pin, duty)` → `hal_pwm_write()`
- [ ] **4.4.5** `bindings/error.c` — **HAL 错误到 OCaml 异常的映射**
  - `hal_err_t` → OCaml `Failure` / `Invalid_argument` / `Sys_error` 的统一转换
  - `hal_cap_supported` 检查 → 不支持的操作抛 `Failure "unsupported"` 而非崩溃
  - 每个 binding 调用 HAL 后检查返回值，`HAL_OK` 以外的错误按语义映射

### 任务 4.5：C → OCaml 回调（`caml_callback`，§7.4）

**目标：** C 侧能反向调用 OCaml 闭包（事件驱动刚需）。

- [ ] **4.5.1** `caml_callback(closure, arg)` 实现
  - 构造合成 APPLY：压入参数 + 返回帧
  - 跳转到闭包 code pointer
  - 返回后恢复寄存器
- [ ] **4.5.2** `caml_callback2`、`caml_callback3`、`caml_callbackN`
  - 多参数版本
- [ ] **4.5.3** GC 根保护（回调入口）
  - `closure` 和所有 `arg` 注册为 GC 根
- [ ] **4.5.4** 回调中异常处理
  - 回调内 RAISE 且无更多 trap → C 侧致命错误
  - 设备端不传播 OCaml 异常到 C

### 任务 4.6：中断与调度（§7.5 + §9.4）

**目标：** ISR 只设置 flag，主循环在安全点轮询触发回调。

- [ ] **4.6.1** ISR 约束实现
  - ISR 中仅设置 `volatile` flag
  - ISR 不调用 VM/FFI/分配/GC
- [ ] **4.6.2** `caml_process_events()`（§9.1）
  - 主循环轮询 ISR flag
  - flag 置位 → `caml_callback` 触发用户注册的处理函数
- [ ] **4.6.3** VM yield 机制（§9.4）
  - 指令计数阈值到达 → `camelino_yield_flag` 置位
  - VM 保存状态（pc/acc/env/sp/trap/extra_args）
  - `caml_interpret()` 返回
  - 平台主循环处理事件后恢复执行
- [ ] **4.6.4** `STOP` 语义
  - VM 设置 `halted = true`
  - 主循环不再恢复 `caml_interpret()`
  - 只跑事件处理

### 任务 4.7：Arduino 平台适配器（`platform/arduino/`，首期，§7.6.4）

**目标：** RP2350 via arduino-pico 完整适配。

- [ ] **4.7.1** `platform_config.h`
  - `CAMELINO_WORD_SIZE = 32`
  - `CAMELINO_BIG_ENDIAN = 0`
  - `CAMELINO_HAS_FPU = 0`（软 FPU）
  - `HEAP_SIZE`、`STACK_SIZE`、`MAX_GLOBALS`（默认值）
- [ ] **4.7.2** `hal_adapter.c` — HAL 接口映射到 Arduino API
  - `hal_gpio_write` → `digitalWrite`
  - `hal_gpio_read` → `digitalRead`
  - `hal_gpio_init` → `pinMode`
  - `hal_uart_write` → `Serial.write`
  - `hal_delay_ms` → `delay`
  - `hal_time_millis` → `millis`
  - ...
- [ ] **4.7.3** `port_init.c`
  - Arduino `setup()` / `loop()` 驱动
  - `setup()` → `caml_memory_init()` + `caml_init_primitives()` + `caml_load_bytecode()` + `caml_init_globals()` + `caml_interpret()`
  - `loop()` → `caml_process_events()` + 若无 halt 则 `caml_interpret()` 恢复

### 任务 4.8：OCaml 端库（`lib/camelino/`）

**目标：** 提供 OCaml 开发者使用的 `external` 声明。

- [ ] **4.8.1** `camelino.ml` 完善
  - 所有 primitive 的 `external` 声明
  - `digital_write`、`digital_read`、`pin_mode`
  - `serial_begin`、`serial_write`、`serial_read`
  - `delay_ms`、`millis`、`micros`
  - `analog_read`、`analog_write`
- [ ] **4.8.2** `camelino.mli` 完善
  - 类型签名与文档注释

### 任务 4.9：示例程序

- [ ] **4.9.1** Blink 示例（已存在骨架，完善）
  - `examples/Blink/Blink.ino` + `examples/Blink/blink.ml`
  - OCaml 代码 `let rec loop () = digital_write LED 1; delay_ms 500; digital_write LED 0; delay_ms 500; loop ()`
- [ ] **4.9.2** Sensor 示例（已存在骨架）
  - ADC 读取 + 串口输出
- [ ] **4.9.3** 编译脚本
  - `ocamlc` + `ocamlclean` + `camelino-embed` 一键生成 `bytecode.h`

### 任务 4.10：Phase 4 内部 C primitive

**目标：** 实现 §1.8 矩阵中标为 Phase 4 的 primitive。

- [ ] **4.10.1** 格式化 primitive
  - `caml_format_int`：`int → string`（十进制格式化，含负数/零处理）
  - `caml_format_float`：`float → string`（%f/%e/%g 格式，注意软 FPU 下精度）
  - `caml_int_of_string`：`string → int`（含 `0x`/`0o`/`0b` 前缀解析）
  - `caml_float_of_string`：`string → float`
- [ ] **4.10.2** `CAMLDEPRIM` 宏定义
  - 定义 `#define CAMLprim value`（声明 primitive 函数签名的前缀宏）
  - 使 C stub 可用 `CAMLprim value caml_xxx(value a) { ... }` 写法

### Phase 4 验收标准

- [ ] OCaml 代码 `digital_write 25 1` 在 RP2350 上点灯
- [ ] 串口读写正常
- [ ] 中断事件回调正常触发
- [ ] Blink 示例完整可运行
- [ ] 所有 HAL 调用不直接依赖 Arduino API（经 `hal_*` 解耦）

---

## Phase 5：完整 ZAM 虚拟机 + stdlib（第 12–16 周）

**核心交付物：** 虚拟机指令集完整（P1 + P2，全量 149 条 ZAM 指令），stdlib 生态可用。

**目标：** 完整 ZAM 指令集，stdlib 的 List.map / Printf 运行。

### 任务 5.1：P1 指令实现（§4.2 P1）

**目标：** 补全完整 OCaml 语义所需指令。

- [ ] **5.1.1** 整数扩展指令
  - `OFFSETINT`、`OFFSETREF`、`ISINT`
  - `ULTINT`（无符号比较，罕见但需支持）
- [ ] **5.1.2** 分支比较指令
  - `BEQ`、`BNEQ`、`BLTINT`、`BGEINT`、`BLEINT`、`BGTINT`
  - `BEQ`/`BNEQ` 等跳转偏移计算
- [ ] **5.1.3** 布尔指令
  - `BOOLNOT`、`BOOLAND`、`BOOLOR`
- [ ] **5.1.4** 循环指令
  - `FORINTLOOP`（for 循环的 ZAM 实现）
- [ ] **5.1.5** 字符串转换指令
  - `STRINGOFINT`、`STRINGOFINT32`
- [ ] **5.1.6** 调试/事件指令（桩实现）
  - `STOP`（§9.4 yield halt）
  - `EVENT`、`BREAK`（开发期 UART trace）
- [ ] **5.1.7** 已有 P0 指令的边界情况修复
  - `DIVINT`/`MODINT` 除零 → 操作数标志位检查
  - 溢出检查完整覆盖

### 任务 5.2：P2 指令实现（§4.2 P2）

**目标：** 覆盖 stdlib 所需的全量 ZAM 指令。

- [ ] **5.2.1** 大常量/大偏移扩展形式
  - `ACC`（大索引）、`PUSH`（大索引）
  - `OFFSET`（大偏移）、`OFFSETCLOSURE`（大偏移）
- [ ] **5.2.3** `ENV` / `CLOSESAM` / `OFFSETCLOSURE` 族指令
  - `ENV`：访问当前闭包环境（不同索引偏移）
  - `CLOSESAM`：创建共享环境的新闭包（infix closure）
  - `OFFSETCLOSURE` / `OFFSETCLOSUREM2` / `OFFSETCLOSURE0`：从闭包偏移访问字段
  - 这些是 OCaml 编译器为嵌套函数/对象生成的指令，缺失会导致含嵌套模块的程序崩溃
- [ ] **5.2.4** 对象/方法调用
  - `GETMETHOD`、`GETPUBMET`、`GETDYNMET`
- [ ] **5.2.5** 剩余所有 ZAM 指令
  - 以 `ocaml/bytecomp/instruct.ml` 全量列表为准
  - 逐条实现 + 差分类验证

### 任务 5.3：stdlib 扩展

**目标：** 更多 OCaml stdlib 模块的字节码可用。

- [ ] **5.3.1** `String` / `Bytes` 模块
  - 无 IO 部分（`length`、`get`、`sub`、`concat`、`compare` 等）
- [ ] **5.3.2** `Printf` 模块（**依赖 channel I/O primitive**）
  - ⚠️ OCaml 的 `Printf` 不是直接调 `serial_write`，而是经 **OCaml channel 系统**：
    - 先实现 `caml_ml_open_descriptor_out`：打开输出通道 → 映射到 UART port
    - 再实现 `caml_ml_output`：写通道 → `hal_uart_write`
    - 再实现 `caml_ml_flush`：刷新通道缓冲
  - 以上三个 primitive 就绪后，OCaml `Printf.printf` → 通道 → `hal_uart_write` → 串口输出
  - `caml_format_int`、`caml_format_float` 提供格式化基础（已在 Phase 4 实现）
  - 浮点格式化在软 FPU 下注意精度
- [ ] **5.3.3** `Buffer` 模块
  - 可变字符缓冲区
- [ ] **5.3.4** `Hashtbl` 模块（**依赖 `Random` 模块**）
  - `Hashtbl` 需要 `Random.int` 做哈希种子/随机化（防 hash DoS）
  - 先实现 `Random` 基本子集：
    - `Random.int`：LCG 算法，用 `hal_time_micros()` 做初始种子
    - `Random.bits`、`Random.float`（可选）
  - 不建议跳过——无随机种子会导致哈希碰撞退化
- [ ] **5.3.5** `Format` 模块
  - 格式化美化打印（依赖 Printf + Buffer）

### 任务 5.4：`ocamlclean` 集成

**目标：** 死代码消除工具链集成。

- [ ] **5.4.1** 评估 `ocamlclean` 与 Camelino 的兼容性
- [ ] **5.4.2** 集成到 `camelino-embed` 工具链
  - 自动运行 `ocamlclean` → 输出裁剪后的 `.cma` → `camelino-embed`
- [ ] **5.4.3** 文档：开发者如何用 `ocamlclean`

### 任务 5.5：Phase 5 差分测试

- [ ] **5.5.1** ZAM 指令全覆盖测试
  - 每类指令至少一个测试用例
  - 边界值测试（最大/最小整数、零、空列表）
- [ ] **5.5.2** stdlib 集成测试
  - `List.map (fun x -> x + 1) [1;2;3]` → `[2;3;4]`
  - `Printf.printf "Hello %d\n" 42` → 串口输出 `Hello 42`
  - `String.concat "," ["a";"b";"c"]` → `"a,b,c"`
- [ ] **5.5.3** 回归测试
  - Phase 1-4 所有测试用例重新验证

### 任务 5.6：Phase 5 内部 C primitive（通道 I/O 等）

**目标：** 实现 §1.8 矩阵中标为 Phase 5 的 primitive，重点是通道 I/O。

- [ ] **5.6.1** 通道 I/O 基础（`caml_ml_input` / `caml_ml_output`）
  - 定义 `caml_channel` 结构体：描述符 + 缓冲 + 状态
  - `caml_ml_output_char`：写一个字符到通道 → 通道缓冲满时 `hal_uart_write` 冲刷
  - `caml_ml_input_char`：从通道读一个字符
  - `caml_ml_flush`：冲刷通道缓冲
  - 这些是 OCaml `Printf` / `Format` / 所有 I/O 的基础——**Printf 不直接调 UART，而是经通道系统**
- [ ] **5.6.2** 通道注册
  - `caml_ml_open_descriptor_out`：映射 UART port 到 OCaml output channel（如 `stdout`）
  - `caml_ml_open_descriptor_in`：映射 UART port 到 OCaml input channel（如 `stdin`）
  - 启动时自动注册 `stdout`/`stderr` → UART port 0
- [ ] **5.6.3** Lazy 惰性值 primitive
  - `caml_update_dummy`：强制 lazy 值
  - `caml_lazy_make_forward`：创建懒求值转发指针
  - 说明：`Lazy_tag`(245) + `Forward_tag`(250) 的处理逻辑

### Phase 5 验收标准

- [ ] 全量 ZAM 指令（149 条）实现完毕
- [ ] List.map / Printf / String 模块差分测试通过
- [ ] `ocamlclean` 工具链集成可用

---

## Phase 6：生态工具 + 多平台移植（第 17–20 周）

**目标：** 生产级工具链，多平台验证，生态可用。

### 任务 6.1：`camelino-check` 静态分析工具（§10.3）

**目标：** PC 端预检工具：在编译到设备前检查兼容性。

- [ ] **6.1.1** 核心分析引擎
  - 解析 `.cma` 字节码
  - 收集所需的 primitive 列表
  - 估算堆使用量（静态分析 + 启发式）
  - 检测对 `Unix`/`Threads`/`Sys` 等不可用模块的依赖
- [ ] **6.1.2** 字长截断风险检测（§3.4.1.1）
  - 扫描全局数据中所有 tagged integer
  - 超 31 位范围 → 警告
- [ ] **6.1.3** 平台能力缺口检查（§7.6.2）
  - 比对所需 HAL capabilities vs 目标平台声明
  - 输出：缺口列表 + 影响范围
- [ ] **6.1.4** CLI 接口
  - `camelino-check --heap 192k --flash 512k --word-size 32 --platform arduino my_app.cma`

### 任务 6.2：主机模拟器完善（`platform/host/`）

**目标：** 完整的 PC 端模拟器，差分测试与开发调试载体。

- [ ] **6.2.1** HAL 实现完善
  - `hal_uart_*` → stdout/stdin 映射
  - `hal_time_*` → `gettimeofday` / `clock_gettime`
  - `hal_gpio_*` → 日志输出 + JSON 状态文件（用于测试断言）
  - `hal_flash_*` → 文件系统模拟
- [ ] **6.2.2** 差分测试自动化脚本
  - 批量运行所有测试用例
  - 对比 `ocamlrun` vs Camelino host 输出
  - CI 集成（GitHub Actions 每次 push 自动运行）
- [ ] **6.2.3** 差分测试输出归一化
  - `ocamlrun` 和 Camelino 输出在 diff 前需归一化：
    - 去除 timing 信息
    - 统一换行符
    - 浮点精度容差（`%.6f` 级别）
  - 输出不一致时自动报告差异位置（行号 + 指令名）
- [ ] **6.2.4** 性能对比
  - 指令执行计数
  - GC 耗时统计

### 任务 6.3：差分测试套件全量（§10.4）

**目标：** 全面覆盖的测试套件。

- [ ] **6.3.1** 测试用例组织
  - 按指令类别分类（算术/函数/异常/GC/IO/stdlib）
  - 每个测试：`.ml` 源文件 + 期望输出
- [ ] **6.3.2** 回归测试框架
  - 一键运行全量测试
  - 失败用例自动收集
- [ ] **6.3.3** 多平台差分验证（自动化）
  - 同一 `.camel` 在 ≥2 平台上分别执行 → 自动比对输出
  - 覆盖组合：32 位小端（RP2350）、64 位小端（host x86）、64 位大端（QEMU s390x 模拟）
  - 验证：字长处理 / 字节序转换 / 浮点格式 / 字符串 padding 全部一致
  - **交叉验证脚本**：一批测试 `.camel` + 多平台跑 + diff → CI matrix job

### 任务 6.4：第二个平台适配器

**目标：** 验证 HAL 解耦设计的正确性——移植一个新平台只需实现 `hal_*`。

- [ ] **6.4.1** 选择移植目标（Pico SDK 或 ESP-IDF）
  - **Pico SDK**：与 Arduino 同 RP2350 硬件，验证纯 C SDK 适配
  - **ESP-IDF**：不同架构（Xtensa/RISC-V），验证跨架构移植
- [ ] **6.4.2** 实现该平台的 HAL 适配器
  - `platform/<name>/hal_adapter.c`
  - `platform/<name>/platform_config.h`
  - `platform/<name>/port_init.c`
- [ ] **6.4.3** 移植检查清单验证（§7.6.4）
  - HAL 函数全部实现
  - 差分测试通过
  - 核心运行时零改动

### 任务 6.5：主机辅助 REPL（`camelino-repl`）

**目标：** PC 端源码编译 + 串口推送 → 设备端字节码执行。

- [ ] **6.5.1** PC 端 CLI 工具 `tools/camelino-repl/`
  - 接收 OCaml 源码 → `ocamlc` 编译 → `ocamlclean` → `camelino-embed` → 生成 `.camel` 片段
  - 串口（UART/USB CDC）发送 `.camel` 到设备
  - 接收设备返回结果并显示
- [ ] **6.5.2** 设备端 REPL 加载器（`src/repl/repl.c`）
  - `caml_repl_receive()`：接收 `.camel` 片段
  - 校验 magic/字长/字节序
  - `caml_load_bytecode()` → `caml_interpret()`
  - 结果回传（`serial_write`）
- [ ] **6.5.3** 行编辑器（`src/repl/line_editor.c`）
  - 终端回显、退格处理
- [ ] **6.5.4** 命令历史（`src/repl/history.c`）
  - 上/下箭头翻历史

### 任务 6.6：构建系统与包管理集成

- [ ] **6.6.1** dune 构建规则
  - `dune build` 一键编译 `lib/camelino/` + 示例
  - 自定义 dune rule：`camelino-embed` 生成 `bytecode.h`
- [ ] **6.6.2** PlatformIO 集成
  - `library.json` 或 `platformio.ini` 示例
  - `pio run -t upload` 一键编译+烧录
- [ ] **6.6.3** `camelino deploy` 命令
  - [ ] **6.6.3.1** 参数解析：`--board`（目标板）、`--port`（串口）、`--platform`（Arduino/pico-sdk...）
  - [ ] **6.6.3.2** 编译 pipeline：`dune build` → `ocamlclean` → `camelino-embed` → `bytecode.h`
  - [ ] **6.6.3.3** 平台编译：调用 `arduino-cli compile` / `pio run` / CMake（按 `--platform` 选择）
  - [ ] **6.6.3.4** 上传/烧录：调用 `arduino-cli upload` / `pio run -t upload` / OpenOCD
  - [ ] **6.6.3.5** 校验回读（可选）：上传后读回 `.camel` 的 CRC32 → 比对确认 Flash 未损坏
- [ ] **6.6.4** CI/CD 完善
  - GitHub Actions：host 模拟器差分测试 + 交叉编译目标（不实际烧录）
  - 多个 target 的 Matrix build（`word_size=32,64` × `endian=LE,BE` × `fpu=soft,hard`）

### 任务 6.7：文档与发布

- [ ] **6.7.1** 用户文档
  - 快速开始指南（Quick Start）
  - API 参考（`Camelino` OCaml 库）
  - 平台移植指南（如何添加新平台）
  - 限制与已知问题（§14）
- [ ] **6.7.2** 开发者文档
  - ZAM 解释器内部结构
  - GC 设计
  - HAL 接口规范
  - 贡献指南
- [ ] **6.7.3** OPAM 包发布
  - `opam install camelino`
  - 版本号 0.1.0（首次发布）

### 任务 6.8：第三方 OPAM 库验证

**目标：** 验证生态兼容路线图 §10.1–10.2。

- [ ] **6.8.1** 选取 3–5 个纯 OCaml OPAM 库
  - 候选：`containers`、`base`（子集）、`yojson`（子集）、`ounit`
- [ ] **6.8.2** 裁剪 + camelino-check 预检
- [ ] **6.8.3** 设备端运行时验证
- [ ] **6.8.4** 记录不兼容模式 → 文档 + camelino-check 规则

### 任务 6.9：C stub 嵌入支持（Design.md §7.3）

**目标：** `camelino-embed` 支持第三方 OPAM 库的 C stub 嵌入。

- [ ] **6.9.1** `ocamlc -custom` 集成
  - 解析 `-custom` 链接产生的 C stub `.o` 文件
  - 提取符号表：哪些 `caml_*` C primitive 来自第三方库
- [ ] **6.9.2** C stub 编译进设备端
  - 将第三方 C stub 源文件加入 Arduino sketch 编译（或生成 wrapper）
  - 确保 C stub 只使用 Camelino runtime API + HAL 接口（§7.3 限制）
- [ ] **6.9.3** 验证：`platform/host/` 上加载带 C stub 的第三方库字节码，差分通过

### Phase 6 验收标准

- [ ] `camelino-check` 输出正确的兼容性报告
- [ ] 差分测试套件全量通过（≥2 平台，≥2 字长/字节序配置）
- [ ] 第二个平台适配器差分测试通过，核心运行时零改动
- [ ] 主机辅助 REPL 工作正常
- [ ] PlatformIO 一键编译+烧录
- [ ] 文档完整，OPAM 包可安装
- [ ] ≥1 个第三方 OPAM 纯 OCaml 库在设备端运行

---

## 任务依赖关系

```
Phase 0  ────→ Phase 1 ────→ Phase 2 ────→ Phase 3 ────→ Phase 4 ────→ Phase 5 ────→ Phase 6
                 │              │              │              │              │              │
                 0.2 基础宏    1.4 解释器    2.1 调用帧    3.1 GC        4.1 FFI       5.1 P1 指令
                 0.3 错误框架  1.5 加载器    2.2 闭包      3.2 字符串    4.3 HAL       5.2 P2 指令
                 0.4 构建系统  1.6 embed      2.3 异常      3.3 浮点      4.4 bindings  5.3 stdlib
                              1.7 差分测试   2.4 全局变量   3.4 其他块    4.5 回调       5.4 ocamlclean
                              1.8 primitive  2.5 启动流程   3.5 stdlib    4.6 中断       ...
                                            2.6 差分测试   3.6 GC 测试   4.7 Arduino
                                            2.7 primitive  3.7 primitive  4.8 OCaml 库
                                                          ...           4.9 示例

跨 Phase 的 primitive 矩阵（§1.8）贯穿 Phase 1–5，各 Phase 按阶段实现对应条目，不得遗漏。

Phase 4 HAL 接口定义后可并行：
  - platform/arduino/
  - platform/host/ 完善
  - platform/picosdk/（Phase 6 启动）

Phase 5 和 Phase 6 的工具链工作可部分并行：
  - camelino-check（6.1）不依赖 Phase 5 完成
  - 差分测试套件（6.3）依赖 Phase 5 指令完整
  - 主机 REPL（6.5）依赖 Phase 5 stdlib（Printf + channel I/O）
  - C stub 嵌入（6.9）依赖 Phase 5 通道 I/O 就绪

注：各 Phase 新增了独立的内部 primitive 实现任务（1.9 / 2.7 / 3.7 / 4.10 / 5.6），按 §1.8 矩阵逐项勾选。
```

---

## 风险与缓解

| 风险 | 等级 | 缓解 |
|------|------|------|
| ZAM 指令集语义偏差（操作数标志位） | 高 | 差分测试逐条对 `ocamlrun`，Phase 1 即搭建 |
| `.cmo` Marshal 格式解析错误 | 高 | 参考 OCaml 源码 `file_formats/cmo_format.ml`，单元测试覆盖 |
| word-size 重写（方案 B）实现复杂 | 中 | 先支持同字长（64→64），再逐步处理 64→32 重写 |
| GC 在嵌入式上的稳定性 | 中 | 先 bump allocator 验证功能，GC 加入后差分测试对比 |
| 软 FPU 性能不足 | 低 | 文档明确，Phase 6 硬 FPU 平台零成本 |
| RP2350 SRAM 不足（520KB） | 中 | `ocamlclean` + 堆大小可配置 + `camelino-check` 预检 |
| 不同 OCaml 版本的字节码格式变化 | 低 | 先锁定 4.14.x，magic/CRC 校验在加载期拒绝不匹配版本 |
| **内部 C primitive 遗漏** | **中** | §1.8 矩阵跟踪 + 每个 Phase 的验收条件要求确认本阶段 primitive 已实现 |
| **Printf 未走 channel I/O** | **中** | 5.3.2 明确经由 `caml_ml_output` 通道，不直接调 UART；Phase 4/5 交接时重点验证 |

---

## 工时估算汇总

| Phase | 内容 | 估算工时 |
|-------|------|----------|
| Phase 0 | 基础设施 | 1 周 |
| Phase 1 | 值表示 + 最小 ZAM | 3 周 |
| Phase 2 | 函数 + 控制流 | 3 周 |
| Phase 3 | GC + 堆块 | 3 周 |
| Phase 4 | FFI + HAL + Arduino 适配器 | 2 周 |
| Phase 5 | 完整 ZAM + stdlib | 5 周 |
| Phase 6 | 生态工具 + 多平台移植 | 4 周 |
| **合计** | | **21 周** |

---

*本文档随项目进展更新。任务完成后将 `[ ]` 改为 `[x]`。*
