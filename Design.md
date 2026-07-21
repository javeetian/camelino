# Camelino 架构设计文档

**版本：** 0.4.0
**目标平台：** 首期 Raspberry Pi Pico 2 W (RP2350，32 位小端、软 FPU)。设计上显式参数化字长（32/64 位）、字节序（大/小端）、浮点（软/硬 FPU），并通过硬件抽象层（HAL）解耦平台 SDK——**不绑定 Arduino**，可移植至 Arduino / Pico SDK / ESP-IDF / Zephyr / 裸机 / 主机模拟器等多平台  
**设备端：** C99 + OCaml 标准字节码解释器（ZAM），通过 HAL 适配多平台  
**开发端：** 标准 OCaml 工具链（`ocamlc` / `ocamlfind` / `opam`）  
**许可证：** MIT

---

## 一、设计目标

Camelino 是一个 **可移植的 OCaml 字节码运行时**，让开发者用标准 OCaml 语言编写嵌入式应用，在微控制器上执行 `ocamlc` 生成的字节码。首期以 Arduino 库形态发布（降低上手门槛），但**核心运行时与平台 SDK 解耦**，后续可平滑移植到 Pico SDK、ESP-IDF、Zephyr、裸机或主机模拟器。

### 1.1 核心原则

| 原则 | 说明 |
|------|------|
| **字节码兼容** | 执行标准 OCaml 字节码（ZAM 指令集），不发明私有 opcode |
| **工具链兼容** | PC 端使用 `ocamlc`、`ocamlfind`、`dune` 等现有工具编译 |
| **生态渐进** | 从最小 stdlib 子集起步，逐步支持更多 OPAM 库 |
| **预编译为主** | 设备端只跑字节码，不做源码编译 |
| **标准 FFI** | 通过 `external` / C stub 绑定硬件，与 OCaml 惯例一致 |
| **平台解耦** | 运行时核心不直接调用任何平台 SDK；硬件/时序/IO 经统一 HAL 接口（§7.6）|

### 1.2 生态兼容策略

「最终 OCaml 生态都能用」分阶段实现，而非一步到位：

```
阶段 A（MVP）     标准 OCaml 语法 + 整数/布尔/函数/异常 + camelino 硬件库
阶段 B（stdlib）  stdlib 字节码子集（List、Array、String、Printf 等）
阶段 C（opam）    纯 OCaml 的 OPAM 库（经 ocamlclean 裁剪后可用）
阶段 D（完整）    自定义平台 shims 替换 Sys/Unix 等桌面依赖，覆盖主流库
```

**设备端无法运行的部分**（通过编译期检测 + 文档明确排除）：

- `ocamlopt` 原生代码（`.cmxs` / `.o`）
- 依赖 pthread / fork / 文件系统的库（`Unix`、`Threads` 等）
- 内存需求超出板载 SRAM 的程序

**设备端可以运行的部分**：

- 所有 `ocamlc` 生成的 `.cmo` / `.cma` 字节码（在 VM 实现完整 ZAM 指令集后）
- 经 `ocamlclean` 死代码消除后的 stdlib 和 OPAM 库
- 通过 `external` 声明 + C stub 绑定的硬件 API 和第三方 C 库

### 1.3 与 OMicroB / OCaPIC 的关系

Camelino 与 [OMicroB](https://github.com/stevenvar/OMicroB)、[OCaPIC](https://github.com/bvaugon/ocapic) 采用相同的理论基础：**Zinc Abstract Machine (ZAM)** 执行 `ocamlc` 字节码。

| 对比项 | OMicroB | Camelino |
|--------|---------|----------|
| 目标平台 | AVR / PIC32 | RP2350 为首期，HAL 解耦多平台 |
| 集成方式 | 独立工具链 `omicrob` | 首期 Arduino Library，可移植至其他 SDK |
| 字节码来源 | `ocamlc` + `ocamlclean` + `bc2c` | `ocamlc` + `ocamlclean` + `camelino-embed` |
| 硬件绑定 | 平台专用 C primitives | 统一 HAL 接口（§7.6），各平台实现适配器 |
| 生态目标 | 嵌入式 OCaml 子集 | 标准 OCaml 生态渐进兼容 |

Camelino 不重复发明字节码格式，参考 OMicroB 论文与 OCaml 官方 `bytecomp/` 实现 ZAM 解释器。

### 1.4 平台可移植性目标（设计约束）

Camelino 的字节码运行时必须在以下维度上**显式参数化**，不能对某一台机器硬编码假设：

| 维度 | 首期（RP2350）| 设计需覆盖 | 处理策略 |
|------|--------------|-----------|----------|
| **字长** | 32 位 | 32 / 64 位 | `value` 用 `intptr_t`；host 编译字长由 embed 重写对齐 target（见 §3.4） |
| **字节序** | 小端 | 小端 / 大端 | 运行时统一从**字节流**按 `CAML_READ_*` 宏读取多字节整数；常量池在 embed 阶段转换为目标序（见 §3.4） |
| **浮点** | 软 FPU（无 FPU） | 软 FPU / 硬 FPU | 运行时只依赖 IEEE-754 `double`；FPU 缺失时由编译器软浮点库提供；`Makeblock`/`GETFLOATFIELD` 不假设对齐（见 §4.2、§5.1） |
| **对齐** | 4 字节 | `sizeof(value)` | 堆分配按 `sizeof(value)` 对齐；指针低位天然为 0 |
| **平台 SDK** | Arduino + Pico SDK | Arduino / Pico SDK / ESP-IDF / Zephyr / 裸机 / 主机 | 核心运行时零 SDK 依赖；硬件/时序/IO 经统一 HAL 接口（§7.6），各平台提供适配器实现 |

> **原则**：任何写成 `*((uint32_t*)p)` 的隐式解引用都属于实现缺陷。多字节整数必须经 `CAML_READ_UINT16/32/64`、`CAML_READ_INT16/32/64` 这类字节序显式的宏；浮点必须经 `CAML_READ_DOUBLE`（含对齐拷贝）。编译期由 `CAMELINO_WORD_SIZE` / `CAMELINO_BIG_ENDIAN` / `CAMELINO_HAS_FPU` 三个宏选择实现。

---

## 二、系统概述

```
┌─────────────────────────────────────────────────────────────────┐
│                         开发端（PC）                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────────┐  │
│  │ .ml 源码 │→ │  ocamlc  │→ │ocamlclean│→ │ camelino-embed  │  │
│  └──────────┘  └──────────┘  └──────────┘  └────────┬────────┘  │
│                                                       │           │
│                                            bytecode.h / .camel    │
└───────────────────────────────────────────────────────┼───────────┘
                                                        │ 烧录 / 串口加载
┌───────────────────────────────────────────────────────┼───────────┐
│                         设备端（目标平台）             ▼           │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                      用户交互层（可选，平台相关）             │  │
│  │   预编译执行（主路径）│ 串口字节码加载 │ 集成层（Arduino/...）  │  │
│  └─────────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │              Camelino 字节码运行时（ZAM 兼容，零 SDK 依赖）    │  │
│  │   ZAM 解释器  │  标记-清扫 GC  │  FFI / C primitive 注册表   │  │
│  └─────────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                      Camelino 核心                             │  │
│  │   值表示（tagged int）│ 堆/栈分配  │  异常（trap stack）       │  │
│  └─────────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │        硬件抽象层（HAL）—— 平台无关接口 + 平台适配器           │  │
│  │   hal_gpio / hal_uart / hal_time / hal_flash ...（接口）      │  │
│  │   ┌──────────┐┌──────────┐┌──────────┐┌──────────┐┌────────┐ │  │
│  │   │ Arduino  ││ Pico SDK ││ ESP-IDF  ││ Zephyr   ││ 裸机/  │ │  │
│  │   │ adapter  ││ adapter  ││ adapter  ││ adapter  ││ host   │ │  │
│  │   └──────────┘└──────────┘└──────────┘└──────────┘└────────┘ │  │
│  └─────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

> **分层契约**：上层（运行时核心 + bindings）**只允许调用 HAL 接口**，不直接 `#include` 任何平台 SDK 头文件。每个目标平台提供一个 `hal_adapter.c`，把 HAL 接口翻译成该平台的 SDK 调用。移植一个新平台 = 实现一组 HAL 函数，核心运行时零改动。

---

## 三、工具链

### 3.1 编译流程

```
my_app.ml
    │
    ├─→ ocamlc -I +camelino/lib -c my_app.ml        # 标准字节码编译
    │
    ├─→ ocamlfind/ocamlc -linkall -o prog.cma ...   # 链接 stdlib + 依赖库
    │
    ├─→ ocamlclean prog.cma                          # 死代码消除（必需）
    │
    ├─→ camelino-embed prog.cma -o bytecode.h        # 提取字节码 + 元数据 → C 头文件
    │
    └─→ Arduino 编译 Blink.ino（#include "bytecode.h"）
              │
              └─→ 烧录到 RP2350
```

### 3.2 `camelino-embed` 工具职责

PC 端 OCaml 工具（`tools/camelino-embed/`），是整个工具链的**链接器**，职责远不止"提取字节码"。完整职责：

#### 3.2.1 输入解析

1. 解析 `.cmo`（单编译单元）与 `.cma`（库归档，含多个 `.cmo` + 索引）—— 二者结构不同，分别处理：
   - `.cmo`：`magic → cu_pos → relocatable bytecode → debug → compilation unit`（Marshal: `cu_code`, `cu_codesize`, `cu_primitives`, `cu_reloc_table`, `cu_required_globals`, `cu_global`...）
   - `.cma`：归档头 + N 个 `.cmo` 体 + 单元索引列表
2. 校验 magic、stdlib CRC、OCaml 版本（见 §3.4.4）。

#### 3.2.2 链接（核心难点）

`.cmo` 中的字节码是 **relocatable** 的——`GETGLOBAL`/`SETGLOBAL`/`GETGLOBALFIELD` 的操作数在链接前是**占位符**，必须靠重定位表在链接时 patch 成最终的全局槽位号。

3. **拓扑排序**：按 `cu_required_globals`（本单元依赖的其他模块全局符号）对所有 CU 做依赖排序。OCaml 模块初始化是**顺序**执行的——每个 CU 的初始化代码 `SETGLOBAL` 自己的模块值，并依赖前序模块已初始化。**排序错误会导致初始化引用未定义全局**。
4. **重定位（RELOC）应用**：对每个 CU，按其 `cu_reloc_table` 遍历字节码，将重定位项 patch 为最终地址/槽位号：
   - `RELOC_GETGLOBAL` → 最终 `global_data` 槽位号
   - `RELOC_SETGLOBAL` → 最终 `global_data` 槽位号
   - `RELOC_GETGLOBALFIELD` → 槽位号 + 字段
   - 代码合并后，所有跳转偏移（`BRANCH` 等）按合并后的基址重算
5. **primitive 解析**：收集所有 CU 引用的 primitive 名（`cu_primitives`），与设备端 C 实现的注册表（§7.2）对齐；**未实现的 primitive 报错**（链接期失败，而非运行期）。

#### 3.2.3 字长/字节序转换（可移植性）

6. 若 host 字长 ≠ target 字长（§3.4.1 方案 B），扫描全局数据做 word-size 重写。
7. 若 host 字节序 ≠ target 字节序（§3.4.2），重写全局数据中的多字节标量为目标序；指令操作数转换为目标序。

#### 3.2.4 输出生成

8. 生成 `bytecode.h`：

```c
// 自动生成，勿手动编辑
#define CAMELINO_BYTECODE_MAGIC "CAMELINO01"
#define CAMELINO_OCAML_VERSION "4.14.1"
#define CAMELINO_WORD_SIZE 32          // §3.4.1
#define CAMELINO_BIG_ENDIAN 0          // §3.4.2

static const uint8_t camelino_code[] = { ... };     // 重定位后的指令流（目标序）
static const uint8_t camelino_data[] = { ... };     // 全局数据初值（目标 word/序）
static const char* camelino_primitives[] = { "caml_camellino_digital_write", ... };

extern void camelino_start(void);  // 入口符号（拓扑序首 CU 的初始化代码起点）
```

> **关键**：`camelino_start` 不是任意偏移，而是**拓扑序第一个 CU 的初始化入口**；其末尾编排为跳转到用户程序的真正 `main`（OCaml 隐式 `_entry`）。

### 3.3 开发者体验目标

```bash
# 最终目标：一条命令完成编译 + 嵌入
dune build
camelino deploy --board pico2 --port COM3

# 或 PlatformIO
pio run -t upload
```

### 3.4 交叉编译与可移植性约束

「设备端只跑字节码」成立的前提是：**字节码在 host 上编译，其布局假设必须与 target 一致**。本节列出硬约束，违反任一条都会导致设备端加载/执行错乱。

#### 3.4.1 字长一致性（word size）

OCaml 字节码的**指令流**（opcode + 操作数）本身与字长无关，但 `.cmo`/`.cma` 中 Marshal 序列化的**全局数据**（`global_data`：常量初值、闭包、浮点块等）按 host 的 `sizeof(value)` 存储：

- 64 位 host → 每个 word 8 字节
- 32 位 target → 每个 word 4 字节

设备端若按 4 字节 word 读取 8 字节编码的全局数据，**整体错位，解释器立即崩溃**。

**二选一的解决方案：**

| 方案 | 做法 | 适用 |
|------|------|------|
| A | 强制用与 target 同字长的 OCaml 工具链编译：`opam sw create ocaml-variants.4.14.1+options,ocaml-option-32bit` | target 为 32 位 |
| **B（采用）** | 允许任意 host 字长，`camelino-embed` 在嵌入阶段做 **word-size 重写**：扫描全局数据，按目标 `word_size` 重新序列化 | 64 位 host → 32/64 位 target 通用 |

**Camelino 采用方案 B。** 这样开发者可继续使用主流的 64 位 OCaml 工具链，由 `camelino-embed` 在 PC 端完成 host → target 的字长/字节序适配，把移植负担集中在工具端而非开发者环境。

#### 3.4.1.1 方案 B 的 word-size 重写算法

`camelino-embed` 读取 `.cmo`/`.cma` 时，**先按 host 字长反序列化**全局数据，再按 target 字长重新序列化。OCaml 的全局数据是一段按 `Wosize`（word 数）度量的内存，其中每个 word 可能是：

| 内容 | host (8B) | target (4B) 重写规则 |
|------|-----------|---------------------|
| tagged integer（低位=1）| `(n<<1)\|1` 占 8B | 截取低 32 位，仍是 `(n<<1)\|1`（OCaml int 在 32 位下为 31 位，需检查是否溢出） |
| 堆块 header（`Hd_val`）| `[Wosize | color | tag]` 占 8B | 重新打包进 32 位 header（见 §5.0）；**Wosize 不变**（仍是 word 数），但每 word 字节数变了，块在字节上的跨度需重算 |
| 块内字段（value）| 逐 word | 递归：整数截断、header 重打包、指针重定位（见下） |
| 字符串 / 浮点块的字节流 | 按 `(Wosize * 8)` 字节连续 | 重新 padding 到 `(Wosize * 4)` 字节，末尾填充字节的编码随 word 大小变化（§5.1 字符串布局） |

**关键难点 — 指针 / 偏移重写：** 全局数据内部、以及 code→data 的引用，都以 **word 偏移**表达。host 8B-word 偏移换算成 target 4B-word 偏移时数值会变化。embed 工具必须：

1. 维护一张 `host_offset → target_offset` 映射（按字节数守恒重算 word 编号）。
2. 对所有 `value` 类型字段判断：若是指针（低位=0），按映射改写为 target word 偏移；若是整数，截断保留。
3. 重定位表（`RELOC_GETGLOBAL` 等）里的**槽位号**与字长无关（是 `global_data` 的 word 索引），需一并换算。

> **整数截断检查**：32 位 target 的 OCaml `int` 是 31 位。重写时若发现某 tagged integer 的有效位超出 31 位范围，embed 工具应**报错**（编译期而非运行期失败），提示该常量无法在 32 位 target 表示。

无论哪种方案，`.camel` 格式头部必须记录 `word_size`（见 §4.4），设备端加载时**校验**，不匹配则拒绝执行。

#### 3.4.2 字节序处理（endianness）

`.cmo`/`.cma` 的全局数据按 **host 字节序**存储。若 host（小端 x86）与 target（大端 MCU）序不同：

- embed 阶段将全局数据中的多字节标量（整数常量、`double`）**重写为目标序**；
- 字节码指令操作数（`CONSTINT` 的 32-bit 操作数、跳转偏移）也需转换为目标序，或在加载阶段按字节流读取。

运行时与 embed 工具统一通过 `CAML_READ_*` / `CAML_WRITE_*` 宏访问多字节整数（见 §1.4），**禁止裸指针解引用**。

#### 3.4.3 浮点（软 FPU / 硬 FPU）

- 运行时只依赖 IEEE-754 `double` 语义，不假设 FPU 存在。
- **无 FPU**（RP2350）：编译器链接软浮点库（`-mfloat-abi=soft`），浮点运算慢 1–2 个数量级（见 §14）。
- **有 FPU** 的 64 位平台：原生 `double` 运算，零额外成本。
- `MAKEFLOAT`/`GETFLOATFIELD`/`SETFLOATFIELD` 不得假设 `double` 的对齐（软浮点 ABI 下 `double` 可能仅 4 字节对齐），读取/写入浮点字段时经 `CAML_READ_DOUBLE` / `memcpy` 到临时变量再操作。
- 浮点数组（tag 254）是 unboxed 的连续 `double`，布局与普通块不同（见 §5.1），VM 按 tag 分流。

#### 3.4.4 二进制兼容校验

`.cmo`/`.cma` 携带若干校验字段，embed 与加载阶段必须全部核对：

| 校验项 | 含义 | 不匹配的处理 |
|--------|------|--------------|
| `.cmo` magic | 编译器版本魔数（如 `Caml1999O011`）| 拒绝 |
| **stdlib CRC** | 编译时所基于的 stdlib 的校验和 | 拒绝（运行时 stdlib 子集必须与编译时一致）|
| `.camel` magic | `CAMELINO01` | 拒绝 |
| `.camel` `word_size` | 编译字长 | 拒绝（除非 embed 已做重写）|
| `.camel` `ocaml_ver` | OCaml 版本字符串 | 拒绝（主版本不一致）|
| `.camel` `checksum` | code+data 校验和 | 拒绝（传输/Flash 损坏）|

**§9.1 的启动流程中，`caml_load_bytecode()` 必须执行上述全部校验，任一失败则 UART 报错并 `camelino_start` 不执行字节码。**

---

## 四、ZAM 虚拟机

### 4.1 架构模型

Camelino 实现 **Zinc Abstract Machine (ZAM)**，与 OCaml 官方 `ocamlrun` 使用相同的执行模型：

| 寄存器/区域 | 说明 |
|-------------|------|
| `pc` | 程序计数器，指向当前 bytecode 指令 |
| `acc` | 累加器，存放当前表达式值 |
| `sp` / `trap` | 操作数栈 / 异常处理器栈 |
| `env` | 当前环境（闭包自由变量） |
| `extra_args` | 多参数函数应用的剩余参数计数 |
| `global_data` | 全局数据区（编译单元初始化后） |
| `heap` | GC 管理的堆 |

### 4.2 标准字节码指令集

Camelino **不定义私有 opcode**，完整实现 OCaml 标准 ZAM 指令集（约 149 条）。  
指令定义以 OCaml 源码 `ocaml/bytecomp/instruct.ml` 和 `ocaml/runtime/instruct.h` 为准。

#### 实现优先级

**P0 — 最小可运行（对应 `ocamlc` 输出简单算术程序）**

| 指令 | 说明 |
|------|------|
| `ACC`, `ACC0`…`ACC7` | 访问栈槽 |
| `PUSH`, `PUSHACC` | 压栈 |
| `POP`, `ASSIGN` | 弹栈 / 赋值 |
| `CONST0`…`CONST3`, `CONSTINT` | 常量 |
| `ADDINT`, `SUBINT`, `MULINT`, `DIVINT`, `MODINT` | 整数运算 |
| `EQ`, `NEQ`, `LTINT`, `LEINT`, `GTINT`, `GEINT` | 比较 |
| `BRANCH`, `BRANCHIF`, `BRANCHIFNOT` | 分支 |
| `RETURN`, `RESTART`, `GRAB` | 函数返回 / 多参数应用 |
| `CLOSURE`, `CLOURSE` | 闭包创建 |
| `APPLY`, `APPTERM`, `PUSHRETADDR`, `RETURN` | 函数调用 |
| `GETGLOBAL`, `SETGLOBAL`, `GETGLOBALFIELD` | 全局变量 |
| `MAKEBLOCK`, `GETFIELD`, `SETFIELD`, `GETVECTITEM`, `SETVECTITEM` | 堆块 / 数组 |
| `C_CALL1`…`C_CALL5`, `C_CALLN` | C primitive 调用（FFI 入口） |

**P1 — 完整 OCaml 语义**

| 指令 | 说明 |
|------|------|
| `PUSHTRAP`, `POPTRAP`, `RAISE`, `RAISE_NOTRACE` | 异常 |
| `OFFSETINT`, `OFFSETREF`, `ISINT`, `ULTINT` | 整数 / ref |
| `BEQ`, `BNEQ`, `BLTINT`, `BGEINT`… | 分支比较 |
| `MAKEFLOAT`, `GETFLOATFIELD`, `SETFLOATFIELD` | 浮点（软/硬 FPU 通用，见 §3.4.3、§5.1；字段读写经 `memcpy`/`CAML_READ_DOUBLE`，不假设对齐） |
| `STRINGOFINT`, `STRINGOFINT32` | 字符串转换 |
| `BOOLNOT`, `BOOLAND`, `BOOLOR` | 布尔 |
| `FORINTLOOP`, `BEQ`, `BNEQ` | 循环 |
| `STOP`, `EVENT`, `BREAK` | 调试 / 事件 |

> **整数运算的操作数标志位**（以 `instruct.h` 为准）：
> - `DIVINT` / `MODINT` 的操作数**最低位**是 **`raise_on_overflow` 标志**：置位时除零抛 `Division_by_zero`，清零时返回 0（OCaml 早期语义）。漏判会导致除零语义错误。
> - `ADDINT` / `SUBINT` / `MULINT` 的操作数最低位是 **`raise_on_overflow`** 标志，溢出时按标志位决定是否抛 `Overflow`。
> - 比较/分支指令（`LTINT` 等）的操作数高位编码**比较种类**（`Comparison_xxx`），不可当作纯整数常量。

**P2 — 完整兼容（覆盖 stdlib 所需）**

- 剩余全部 ZAM 指令
- 对象 / 方法调用（`GETMETHOD`, `GETPUBMET`, `GETDYNMET` 等）
- 大常量 / 大偏移（`ACC`, `PUSH`, `OFFSET`, `OFFSETCLOSURE` 扩展形式）

### 4.3 多参数应用（GRAB 机制）

OCaml 函数应用使用 ZAM 的 **GRAB 优化机制**（与 `ocamlrun` 一致）：

```
应用 f a b c 时：
  1. 依次编译 a, b, c 到栈上（右到左求值）
  2. APPLY 跳转到 f 的代码
  3. GRAB(n) 检查 extra_args：
     - 参数刚好 → 直接执行函数体
     - 参数不足 → 创建部分应用闭包
     - 参数过多 → 执行函数体后对返回值继续 APPLY
```

这是 stdlib 中多参数函数（如 `List.map2`）正常工作的前提，必须在 Phase 2 实现。

### 4.4 字节码文件格式

设备端加载器支持两种输入：

**格式 1：标准 `.cmo`（开发 / 串口加载）**

```
magic number          (Config.cmo_magic_number, 如 "Caml1999O011")
cu_pos                (compilation unit 描述符偏移)
relocatable bytecode  (ZAM 指令序列)
debug info            (可选)
compilation unit      (Marshal: cu_code, cu_codesize, cu_primitives, ...)
```

**格式 2：`.camel` 嵌入式格式（生产 / Arduino 集成）**

```
magic:        "CAMELINO01"
version:      uint16
ocaml_ver:    string
word_size:    uint8         // §3.4.1：编译时的 sizeof(value)（4 或 8）
big_endian:   uint8         // §3.4.2：1=大端，0=小端
code_size + code[]          // 重定位后的指令流（目标序）
data_size + data[]          // 全局数据初值（目标 word/序）
num_prims + prim_names[]    // primitive 名表
entry_offset                // 拓扑序首 CU 初始化入口
checksum                    // code+data 校验和（CRC32）
```

设备端加载时核对 `word_size` / `big_endian` 与运行时编译配置，不匹配拒绝执行（§3.4.4）。

### 4.5 调用栈帧与 Trap 帧布局

ZAM **没有独立的调用栈**——返回地址、环境、异常处理器全部存在**值栈**（`sp`）上。这些帧布局**必须与 `ocamlrun` 完全一致**，否则 `APPLY`/`RETURN`/`APPTERM`（尾调用）、`PUSHTRAP`/`RAISE` 全部错乱。以下以 4.14 为准。

#### 4.5.1 返回帧（`PUSHRETADDR` / `RETURN`）

函数调用时，`PUSHRETADDR` 在值栈上压入一个 **3-slot 返回帧**：

```
sp →  [ saved_extra_args ]   ← RETURN 读取后恢复 extra_args
       [ saved_env        ]   ← RETURN 读取后恢复 env
       [ saved_pc (ret)   ]   ← RETURN 读取后跳回
```

- `APPLY`：把 `extra_args` 设为实际参数数 - 1，压入返回帧，跳转到闭包代码指针。
- `RETURN n`：弹出 n 个参数 + 上述 3-slot 帧，恢复 `extra_args`/`env`/`pc`，acc 保留返回值。
- `APPTERM(n, slots)`（尾调用）：丢弃当前帧，重用栈空间后跳转——是**尾调用优化**的关键，必须正确计算丢弃的 slot 数，否则栈无限增长或被破坏。
- `RESTART`：配合 `GRAB` 处理多余参数，从栈上重新组织参数帧。

#### 4.5.2 Trap 帧（`PUSHTRAP` / `POPTRAP` / `RAISE`）

`PUSHTRAP` 在值栈上压入一个 **4-slot 异常处理帧**：

```
sp →  [ handler_pc      ]   ← RAISE 时跳转到的异常处理代码
       [ saved_sp_offset ]   ← 恢复 sp（丢弃栈上临时数据）
       [ saved_env       ]   ← 恢复 env
       [ saved_extra_args]   ← 恢复 extra_args
```

- `RAISE`：找到栈顶最近的 trap 帧，按 `saved_sp_offset` 回退 sp，恢复 `env`/`extra_args`，acc 设为异常值，跳转到 `handler_pc`。
- `RAISE_NOTRACE`：同上但跳过 backtrace 记录（设备端可等同 RAISE）。
- `POPTRAP`：弹出 4-slot 帧。
- trap 帧通过 `trap` 寄存器（§4.1）串联；栈展开时遍历至匹配的 handler。

#### 4.5.3 闭包帧访问（`ENV` / `CLOSESAM` / `APPLY`）

闭包本身在堆上（§5.2），但当前执行的闭包指针由 `env` 寄存器持有；`ACC`/`PUSHACC` 的索引 0 表示 `env`（即闭包自身），索引 ≥1 表示从 `env+1` 起的自由变量（`Start_env = 2`，跳过 code pointer 与 closinfo）。

---

## 五、值表示系统

遵循 OCaml 官方 runtime 的 **tagged integer** 约定（与 `ocaml/values.h` 一致）。**最低位约定**：**整数低位 = 1**，**指针低位 = 0**（与官方一致；对齐保证指针低位天然为 0，指针无需额外编码）。

```c
typedef intptr_t value;          // 与 sizeof(void*) 等宽

#define Is_long(x)   (((x) & 1) != 0)                 // 低位=1 → 整数
#define Is_block(x)  (((x) & 1) == 0)                 // 低位=0 → 指针
#define Long_val(x)  ((x) >> 1)
#define Val_long(x)  ((((value)(intptr_t)(x)) << 1) | 1)
#define Val_int(x)   Val_long(x)

// 堆块：value 指向第一个字段，header 在 block[-1]
#define Hd_val(b)    (((header_t*)(value*)(b))[-1])
#define Field(x, i)  (((value*)(x))[i])
```

> **命名规范：** 设备端 runtime 使用 OCaml 官方命名（`Val_long` / `Long_val` / `Field`），  
> 当前 `value.h` 中的 `Caml_*` 前缀将在整合时统一重命名。
>
> **可移植性：** `value` 用 `intptr_t` 而非 `uintptr_t`，字长随平台（32/64 位）；所有多字节整数读写经 `CAML_READ_*` 宏（§1.4），不假设字节序。

### 5.0 堆块 header 布局（含 GC color）

header 是一个 **word**（与 `value` 等宽），布局与官方一致，**高→低 = `Wosize` | `Color(2bit)` | `Tag(8bit)`**：

```
word: [ Wosize (高位) ... | Color(2) | Tag(8 低位) ]

  Tag   (低 8 位)：块类型（见 §5.1）
  Color (次低 2 位)：GC 颜色标记，Mark-Sweep 必需
        CAML_WHITE = 0  可回收
        CAML_GRAY  = 1  待扫描
        CAML_BLACK  = 2  已扫描存活
  Wosize (剩余高位)：块内字段数（word 数），不含 header
```

```c
typedef value header_t;
#define Wosize_hd(hd)   ((mlsize_t)((hd) >> 10))
#define Tag_hd(hd)      ((tag_t)((hd) & 0xFF))
#define Hd_with_wosize(w)  ...
#define Make_header(wosize, tag, color) \
    (((header_t)(wosize) << 10) | ((header_t)(color) << 8) | (tag))
```

> **注意 header 宽度随字长变化**：32 位 header 的 `Wosize` 为 22 位，64 位为 54 位；`Color`/`Tag` 位段固定。embed 工具做 word-size 重写时（§3.4.1.1）必须重新打包 header。

### 5.1 堆块标签（与 OCaml 官方 4.14 对齐）

| Tag | 常量名 | 含义 | 备注 |
|-----|--------|------|------|
| 0–244 | `0` | 结构化块（元组、记录、普通变体、数组）| 通用 |
| 245 | `Lazy_tag` | 悬垂 lazy | |
| **247** | `Closure_tag` | **闭包** | 见 §5.2 |
| 248 | `Object_tag` | 对象 | |
| 249 | `Infix_tag` | 内嵌闭包（infix）| 多闭包共享块 |
| 250 | `Forward_tag` | 转发指针 | |
| 251 | `Abstract_tag` | 抽象块（不透明 C 数据）| GC 不扫描字段 |
| **252** | `String_tag` | **字符串 / Bytes** | 见 §5.3 |
| **253** | `Double_tag` | 单个 boxed `double` | |
| **254** | `Double_array_tag` | unboxed 浮点数组 | 见 §5.4 |
| 255 | `Custom_tag` | 自定义块（带 finalizer/比较/哈希 vtable）| |

> **历史教训**：早期文档误用 `0..6` 的自创 tag 枚举，与官方完全不符。**closure 必须用 247、string 用 252、double_array 用 254**，否则 `ocamlc` 生成的 `MAKEBLOCK(247)` 创建的闭包会被误判，闭包调用彻底失效。

### 5.2 闭包表示（OCaml ≥ 4.12，含 closinfo）

> ⚠️ 旧文档 `field0=code, field1..n=fv` 是 **4.11 及以前**的布局，对 4.14 target **错误**。

4.12 起，闭包块（tag 247）布局：

```
closure block (tag = 247):
  field 0: code pointer     (bytecode 偏移)
  field 1: closinfo         (编码 arity + closure kind)
  field 2..n: free variables (环境，Start_env = 2)
```

`closinfo` 是一个 `value`（实际是 tagged integer 风格的打包字），编码：

- **arity**：函数期望的参数个数（`GRAB`/`APPLY` 据此判断）
- **closure kind**：`Pure`（纯，不访问环境/可尾调用）/ `Default`（普通）

VM 在 `APPLY` 时读 `closinfo` 得到 arity，与 `extra_args + 1` 比较：相等则执行函数体；不足则 `GRAB` 创建部分应用闭包；过多则执行后对返回值继续 `APPLY`（currying）。

`Start_env = 2`：自由变量从 `field[2]` 开始（跳过 code pointer 与 closinfo）。`ACC0`/`PUSHACC0`（索引 0）指向 `env`（即闭包自身），索引 ≥2 才是自由变量。

### 5.3 字符串布局（tag 252）

字符串是连续字节块，**长度不显式存储**，而是靠末尾填充字节编码：

```
块大小（Wosize）= (len / sizeof(value)) + 1   （向上取整到 word 边界）
最后一个字节 = padding count（1..sizeof(value)）
```

例（4-byte word，`"abc"`）：`[ 'a' 'b' 'c' \x01 ]` → Wosize=1，末字节 `\x01` 表示有 1 字节填充。  
`caml_alloc_string` 必须复制此编码，否则 `String.length` / 复制 / 比较 / word-size 重写（§3.4.1.1）全部出错。

### 5.4 浮点数组布局（tag 254，unboxed）

浮点数组字段是**连续的 raw `double`**（每个占 2 word），`GETFLOATFIELD`/`SETFLOATFIELD` 按字节偏移读取，与普通 `GETFIELD`（按 word）不同：

```c
double GETFLOATFIELD(value v, mlsize_t i) {
    double d;
    memcpy(&d, (char*)v + i * sizeof(double), sizeof(double));  // 不假设对齐
    return d;
}
```

VM 在 `GETFIELD`/`SETFIELD` 前必须检查 tag：tag==254 走浮点路径，否则按 word 访问。软 FPU 下 `double` 可能仅 4 字节对齐，**必须 `memcpy`，禁止解引用**（§3.4.3）。

### 5.5 boxed double（tag 253）

单个 `float` 装箱为 tag 253 块（Wosize = `sizeof(double)/sizeof(value)`），布局与浮点数组单元素相同，经 `MAKEFLOAT`/`MAKEFLOATBLOCK` 创建。

---

## 六、内存管理

### 6.1 设计原则

- **不使用硬编码 SRAM 地址**，由链接器分配静态内存池
- **不使用系统 malloc**，避免与 Arduino SDK 堆冲突
- **GC 在分配时触发**，不在主循环盲目调用

### 6.2 内存池布局（平台无关，大小由 `platform_config.h` 配置）

```c
// core/memory.c — 静态池，大小由各平台 platform_config.h 提供
// platform/<name>/platform_config.h 定义 HEAP_SIZE / STACK_SIZE / MAX_GLOBALS
static uint8_t camelino_heap[HEAP_SIZE];       // 首期 RP2350 默认 192KB
static value   camelino_stack[STACK_SIZE];     // 默认 8192 slots
static value   camelino_globals[MAX_GLOBALS];  // 默认 4096
```

```
┌─────────────────────────────────────────────┐
│  平台 SDK 静态数据 + C 栈（链接器管理）         │
├─────────────────────────────────────────────┤
│  camelino_globals[]    全局变量区              │
├─────────────────────────────────────────────┤
│  camelino_stack[]      ZAM 操作数栈            │
├─────────────────────────────────────────────┤
│  camelino_heap[]       Mark-Sweep GC 堆       │
├─────────────────────────────────────────────┤
│  字节码 + 常量（Flash / PROGMEM，不占 SRAM）    │
├─────────────────────────────────────────────┤
│  REPL / UART 缓冲      4KB（可选）             │
└─────────────────────────────────────────────┘
```

### 6.3 GC 策略

| 项目 | 选择 |
|------|------|
| 算法 | Mark-Sweep（与 OMicroB 一致，适合嵌入式） |
| 触发 | 堆使用量超过阈值，或 `caml_alloc` 失败时 |
| 根集合 | 栈、全局变量、寄存器（acc, env）、trap 链 |
| 颜色标记 | header 的 `Color` 位（§5.0）：WHITE 可回收 / GRAY 待扫描 / BLACK 存活 |
| 安全点 | `C_CALL*` 返回后、主循环 iteration 开始前（与调度 yield 复用，§9.4） |
| ISR | **禁止**在 ISR 中调用 VM/FFI/分配或触发 GC（§7.4） |

### 6.4 错误与资源耗尽

| 情形 | 设备端处理 |
|------|-----------|
| 栈溢出（`sp` 越过 guard / `STACK_SIZE`）| 抛 `Stack_overflow`；`sp` 检查在 `PUSH`/`PUSHRETADDR`/`PUSHTRAP`/`APPLY` 处进行 |
| 堆耗尽（GC 后 `caml_alloc` 仍失败）| 抛 `Out_of_memory`；若栈/trap 也无法处理，进入 UART fatal 错误并 `STOP` |
| 字节码校验失败（magic/版本/CRC/prim 缺失）| `caml_load_bytecode()` 返回错误码，`camelino_start` 不执行（§9.1）|
| 除零 / 整数溢出 | 按操作数 `raise_on_overflow` 标志抛 `Division_by_zero` / `Overflow`（§4.2）|

---

## 七、FFI 与硬件绑定

### 7.1 标准 OCaml FFI 模型

OCaml 通过 `external` 声明调用 C 函数，编译为 `C_CALLN` 字节码 + primitive 名称：

```ocaml
(* lib/camelino/camelino.ml *)
external digital_write : int -> int -> unit = "caml_camellino_digital_write"
external digital_read  : int -> int         = "caml_camellino_digital_read"
external delay_ms      : int -> unit        = "caml_camellino_delay_ms"
external millis        : unit -> int        = "caml_camellino_millis"
```

```c
// bindings/gpio.c —— CAMLprim 包装层：只做 value<->C 转换 + 调 HAL，不直接碰平台 API
#include "caml/alloc.h"     // Camelino runtime 头
#include "hal/gpio.h"       // 平台无关 HAL 接口（§7.6）

CAMLprim value caml_camellino_digital_write(value pin, value v) {
    CAMLparam2(pin, v);
    hal_gpio_write(Long_val(pin), Long_val(v) != 0);   // 经 HAL，而非 digitalWrite()
    CAMLreturn(Val_unit);
}
```

设备端启动时，`camelino-embed` 生成的 primitive 表与 `caml_register_global_primitive` 对齐。bindings 层不感知具体平台——它只调用 `hal_*`，由平台适配器把 `hal_*` 翻译成实际 SDK 调用（§7.6）。

### 7.2 Primitive 注册

```
启动时：
  caml_init_primitives()
    ├─→ 注册 camelino 内置 primitives（gpio, serial, time...）
    ├─→ 加载 bytecode.h 中的 prim_names[]
    └─→ 校验：bytecode 引用的每个 prim 都有 C 实现
```

### 7.3 第三方 OPAM 库的 C stub

支持标准 OCaml C stub 模式，让带 C 扩展的 OPAM 库在裁剪后可用：

```
opam install some-lib          # PC 端
ocamlc -custom ...             # 链接 C stub .o
ocamlclean                     # 裁剪
camelino-embed                 # 嵌入（C stub 编译进 Arduino sketch）
```

限制：C stub 只能使用 Camelino 提供的 runtime API，不能依赖 `libc` 完整实现。

### 7.4 C → OCaml 回调（`caml_callback`）—— 硬件事件驱动刚需

§7.1–7.3 只覆盖 OCaml → C。但硬件交互最典型的模式是**事件回调**（如 `attachInterrupt(pin, handler)`——把一个 OCaml 闭包注册给中断/事件，C 侧在事件发生时反向调用它）。因此 runtime 必须提供 C → OCaml 的回调入口：

```c
// runtime 提供的回调 API（与官方 runtime/lexlf.h 一致）
value caml_callback(value closure, value arg);
value caml_callback2(value closure, value arg1, value arg2);
value caml_callback3(value closure, value arg1, value arg2, value arg3);
value caml_callbackN(value closure, int narg, value args[]);
```

**实现要点（4.14 + `caml_callback` 复用 VM 值栈）：**

1. 回调构造一次合成的 `APPLY`：压入参数 + 返回帧（§4.5.1），跳转到闭包 code pointer 执行；返回后恢复寄存器。
2. **GC 根保护**：回调入口必须用 `CAMLparam`/`CAMLlocal` 注册 `closure` 与所有 `arg` 为 GC 根，否则回调过程中 GC 可能移动/回收它们。
3. **复用 VM 值栈**：回调的返回帧压在**主 VM 的值栈**上，因此 C 调用 OCaml 的递归深度受 `STACK_SIZE` 限制；连续嵌套回调会逼近栈底，需 `Stack_overflow` 检查（§6.4）。
4. **回调中可触发 GC/分配**：与 §6.3 安全点一致。
5. **回调中抛出的 OCaml 异常**：若回调内 `RAISE` 且栈顶无更多 trap，转为 C 侧的致命错误（设备端不传播 OCaml 异常到 C）。

### 7.5 中断 / 重入约束

VM 的值栈、寄存器（acc/env/sp/trap/extra_args）、GC 状态均为**全局单例，非可重入**。约束：

- **ISR（中断服务例程）中禁止**：调用 VM、调用任何 `caml_*` / `CAMLprim`、分配、触发 GC。
- ISR 唯一允许的操作：设置一个 **flag**（如 `volatile uint8_t pin_irq_flag`）。
- 主循环（平台主循环 → VM yield，§9.4）在安全点轮询 flag，**在 VM 上下文中**通过 `caml_callback` 触发用户注册的 OCaml 处理函数。

### 7.6 硬件抽象层（HAL）—— 多平台移植的核心

为支持 Arduino / Pico SDK / ESP-IDF / Zephyr / 裸机等多平台，Camelino 在「C primitive（§7.1）」与「平台 SDK」之间插入一层 **平台无关的 HAL 接口**。所有硬件/时序/IO 能力经此接口暴露，各平台提供适配器实现。

#### 7.6.1 分层与依赖方向

```
OCaml 程序
   │ external digital_write : ... = "caml_camellino_digital_write"
   ▼
bindings/*.c   ← CAMLprim 包装层（OCaml value <-> C 参数转换，GC 根）
   │ 只调用 hal_*
   ▼
hal/hal_*.h    ← 平台无关 HAL 接口（纯声明，零 SDK 头文件）
   ▲
   │ 各平台实现
platform/<name>/hal_adapter.c   ← 调用该平台 SDK（Arduino API / pico-sdk / esp-idf / ...）
   ▼
平台 SDK（Arduino / pico-sdk / esp-idf / Zephyr / CMSIS / 裸机寄存器）
```

> **铁律**：`bindings/` 与 `core/` 中的代码**禁止直接调用** `digitalWrite` / `uart_putc` / `delay` 等任何平台 API，也禁止 `#include <Arduino.h>` 等平台头文件。一切硬件能力必须经 `hal_*` 接口。违反此规则的代码无法跨平台。

#### 7.6.2 HAL 接口设计原则

1. **能力而非实现**：接口描述"做什么"（`hal_gpio_write`），不暴露"怎么做"（不出现 `GPIOREGS->OUT_SET`）。
2. **同步阻塞语义**：HAL 函数默认同步阻塞（`hal_uart_write` 等发完才返回）；异步/中断驱动由上层（事件模型 §9.4）处理，HAL 不回调 VM。
3. **错误统一返回码**：所有可能失败的 HAL 调用返回 `hal_err_t`（`HAL_OK` / `HAL_ERR_INVAL` / `HAL_ERR_TIMEOUT` / `HAL_ERR_UNSUPPORTED`...），bindings 层决定如何映射到 OCaml（异常或 `result`）。
4. **能力探测**：提供 `hal_cap_supported(cap_id)` 让平台声明它实现了哪些能力；未实现的 capability 对应的 primitive 在注册期标记为 `unsupported`，运行时调用时抛 `Failure`（而非链接失败）——这样同一份字节码可在能力不同的平台上降级运行。
5. **无动态分配**：HAL 不分配 OCaml 堆，不持有 GC 根；资源句柄用平台定义的不透明整数（`hal_handle_t`）。

#### 7.6.3 HAL 接口清单（首期，随阶段扩展）

```c
/* hal/gpio.h */
typedef enum { HAL_GPIO_INPUT, HAL_GPIO_INPUT_PULLUP, HAL_GPIO_INPUT_PULLDOWN,
               HAL_GPIO_OUTPUT, HAL_GPIO_OUTPUT_OD } hal_gpio_mode_t;
hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode);
hal_err_t hal_gpio_write(uint32_t pin, bool value);
bool      hal_gpio_read(uint32_t pin);
hal_err_t hal_gpio_toggle(uint32_t pin);

/* hal/uart.h */
typedef struct hal_uart_config { uint32_t baud; uint32_t tx_pin; uint32_t rx_pin; } hal_uart_config_t;
hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg);
int       hal_uart_write(uint8_t port, const uint8_t* data, size_t len);   // 返回写入字节数
int       hal_uart_read(uint8_t port, uint8_t* buf, size_t len);            // 非阻塞，返回读取字节数
void      hal_uart_flush(uint8_t port);

/* hal/time.h */
uint64_t  hal_time_millis(void);   // 单调毫秒计数
uint64_t  hal_time_micros(void);
void      hal_delay_ms(uint32_t ms);
void      hal_delay_us(uint32_t us);

/* hal/analog.h */
hal_err_t hal_adc_init(uint32_t pin);
uint32_t  hal_adc_read(uint32_t pin);       // 返回原始值，位数由 hal_adc_resolution 决定
hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz);
hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty);   // duty 为 [0, HAL_PWM_MAX]

/* hal/flash.h —— 字节码与持久化存储 */
hal_err_t hal_flash_read(uint32_t offset, uint8_t* buf, size_t len);
hal_err_t hal_flash_write(uint32_t offset, const uint8_t* buf, size_t len);
hal_err_t hal_flash_erase(uint32_t offset, size_t len);

/* hal/interrupt.h —— 仅注册 ISR flag，不直接进 VM */
hal_err_t hal_gpio_attach_isr(uint32_t pin, hal_gpio_edge_t edge,
                              hal_isr_flag_t* flag);
hal_err_t hal_gpio_detach_isr(uint32_t pin);
bool      hal_isr_flag_take(hal_isr_flag_t* flag);   // 原子取走并清零

/* hal/cap.h —— 能力探测 */
bool      hal_cap_supported(hal_cap_id_t cap);        // HAL_CAP_UART / HAL_CAP_FLASH / ...
```

#### 7.6.4 平台适配器（port）结构

每个移植目标在 `platform/<name>/` 下提供：

```
platform/
├── arduino/        # 首期：Arduino Core (含 RP2350 via arduino-pico)
│   ├── hal_adapter.c     # hal_gpio → digitalWrite, hal_uart → Serial, ...
│   ├── platform_config.h # CAMELINO_WORD_SIZE, HEAP_SIZE, 栈/全局区大小
│   └── port_init.c       # main → setup()/loop() 或等价物
├── picosdk/        # pico-sdk 裸 API
├── espidf/         # ESP-IDF
├── zephyr/         # Zephyr RTOS
├── baremetal/      # 裸机（寄存器直写，用于教学/极简目标）
└── host/           # 主机模拟器（PC 上跑字节码，用于开发与差分测试 §10.4）
```

**移植检查清单**（新平台必须全部满足）：

1. 实现所声明 capability 对应的全部 `hal_*` 函数；
2. 提供 `platform_config.h`（字长/字节序/FPU 宏、堆栈全局区大小、entry）；
3. 提供 `port_init.c`（如何从平台入口进入 Camelino：Arduino 的 `setup()/loop()`、Zephyr 的 thread、裸机的 `main()`）；
4. 通过该平台的差分测试（§10.4）。

---

## 八、模块划分

| 模块 | 文件 | 职责 |
|------|------|------|
| 值表示 | `core/value.h/c` | OCaml value 编码，堆块操作 |
| 内存管理 | `core/memory.h/c` | 堆分配，Mark-Sweep GC |
| 字节码 | `core/bytecode.h/c` | `.cmo` / `.camel` 加载与验证 |
| 操作码 | `core/opcodes.h` | ZAM 指令常量（同步 OCaml 版本） |
| 解释器 | `core/vm.h/c` | ZAM 主循环，寄存器，trap |
| FFI | `ffi/ffi.h/c`, `ffi/primitives.c` | C primitive 注册与 dispatch |
| 硬件绑定 | `bindings/*.c` | OCaml `CAMLprim` 包装层（**只调用 HAL**） |
| HAL 接口 | `hal/hal_*.h` | 平台无关硬件/时序/IO 接口声明 |
| 平台适配 | `platform/<name>/hal_adapter.c` | 各平台 HAL 实现 + `platform_config.h` + `port_init.c` |
| REPL | `repl/repl.h/c` | 串口字节码加载（非源码编译） |
| 集成层 | `Camelino.h/cpp`（Arduino）/ `main.c`（其他）| 平台入口，可选 |

---

## 九、执行流程

### 9.1 启动流程

```
setup()                          // Arduino 形态；其他平台见 §9.4 的 port_init.c
  │
  ├─→ caml_memory_init()           // 初始化堆、栈、全局区
  ├─→ caml_init_primitives()       // 注册 C primitives（绑定层 → HAL）
  ├─→ caml_load_bytecode()         // 从 Flash 加载 bytecode.h，校验 magic/版本/CRC/字长/字节序（§3.4.4）
  ├─→ caml_init_globals()          // 按拓扑序执行各 CU 初始化（SETGLOBAL，§3.2.2）
  └─→ camelino_start()             // 跳转到 OCaml 程序入口

loop()
  │
  ├─→ caml_process_events()        // 轮询 ISR flag → caml_callback 触发用户处理（§7.4/§7.5）
  └─→ 推进 OCaml 主逻辑（见 §9.4 调度模型）
```

### 9.2 ZAM 执行循环

```
caml_interpret()
  │
  └─→ for (;;) {
        switch (*pc++) {
          case ACC:       acc = sp[n]; break;
          case PUSH:      *++sp = acc; break;
          case CONSTINT:  acc = Val_long(CAML_READ_INT32(pc)); pc += 4; break;
          case ADDINT:    acc = Val_long(Long_val(acc) + Long_val(*sp--)); break;
          case GRAB:      /* 多参数应用，读 closinfo（§5.2）*/ break;
          case C_CALL1:   acc = prim_table[id](acc); break;
          case RAISE:     /* trap 帧展开（§4.5.2）*/ break;
          ...
        }
        if (--yield_counter <= 0) { save_vm_state(); return; }  // §9.4 调度 yield
      }
```

> 操作数经 `CAML_READ_INT32`（字节序显式，§1.4）；每 N 条指令检查 yield flag，把控制交还**平台主循环**（Arduino `loop()` / Zephyr thread loop / 裸机 `while(1)`，§9.4）。


### 9.3 REPL 流程（主机辅助，非设备端编译）

```
设备端 REPL 只做字节码加载，不做 OCaml 源码编译：

  PC: ocamlc + camelino-embed → 生成 .camel 片段
       │
       │ 串口发送
       ▼
  设备: caml_repl_receive() → 校验 magic/字长/字节序 → caml_load_bytecode() → caml_interpret()

完整交互式 REPL（源码级）需 PC 端工具配合：
  PC: camelino-repl --port COM3   （接收源码 → 编译 → 回传字节码 → 显示结果）
```

### 9.4 调度模型：VM 与平台主循环的协作

OCaml 程序可能写死循环（如 `let rec blink () = ...; blink ()`），也可能只做一次性初始化后返回。为统一两种情况、并让平台主循环能处理串口/中断事件，Camelino 采用 **显式 yield / 协作式调度**：

- **VM 在安全点检查 `camelino_yield_flag`**（指令计数阈值，如每 N 条指令或每个 `C_CALL*` 返回后）。flag 置位时，VM 把当前执行状态（pc/acc/env/sp/trap/extra_args）保存到 VM 上下文结构体，从 `caml_interpret()` 返回，把控制交还平台主循环。
- 平台主循环执行完事件处理（§9.1 `caml_process_events`）后，若 OCaml 主逻辑未结束，再次调用 `caml_interpret()` 恢复执行。
- **平台无关**：`caml_interpret()` / `caml_process_events()` / VM 上下文保存恢复是平台无关的核心 API；由各平台的 `port_init.c` 决定如何驱动它们：
  - Arduino：`setup()` 调初始化，`loop()` 调 `caml_process_events()` + `caml_interpret()`。
  - Zephyr/FreeRTOS：在专用 thread 的 `while(1)` 里轮询。
  - 裸机：`main()` 的 `while(1)`。
  - 主机模拟器：同裸机，便于差分测试（§10.4）。
- **STOP 语义**（§9.2）：OCaml `_entry` 末尾的 `STOP` 不结束进程，而是**永久让出控制**——VM 设置 `halted = true`，主循环不再恢复 `caml_interpret()`，只跑事件处理。
- **安全点与 GC 复用**：同一套"指令计数阈值 + 状态保存"机制既服务 yield，也服务 GC 安全点（§6.3），一处实现两用。

> **反例（不采用）**：若 `camelino_start()` 内含死循环且不 yield，则平台主循环永不执行，串口/中断全部失效——与 Blink 示例的需求矛盾，故放弃。

---

## 十、生态兼容路线图

### 10.1 stdlib 支持计划

| OCaml 模块 | 支持阶段 | 说明 |
|------------|----------|------|
| `Pervasives` / `Stdlib` | Phase 2 | 整数、布尔、比较、引用 |
| `List` | Phase 3 | 链表（纯函数，无 IO） |
| `Array` | Phase 3 | 数组 |
| `String` / `Bytes` | Phase 3 | 字符串（无 IO 部分） |
| `Printf` | Phase 4 | 需绑定 UART 输出 |
| `Buffer` | Phase 4 | 缓冲区 |
| `Hashtbl` | Phase 5 | 哈希表 |
| `Format` | Phase 6 | 格式化（依赖 Printf） |
| `Sys` | 平台 shim | 仅提供 `time`, `catch_break` 等子集 |
| `Unix` / `Threads` | 不支持 | 桌面专用 |

### 10.2 OPAM 库兼容条件

一个 OPAM 库可以在 Camelino 上运行的条件：

1. 仅依赖 `ocamlc` 字节码（非 `ocamlopt`）
2. 不依赖 `Unix`、`Threads`、`Bigarray`（除非有 shim）
3. 经 `ocamlclean` 后体积 ≤ Flash 容量
4. 运行时堆需求 ≤ 配置的 `HEAP_SIZE`
5. C stub（如有）仅使用 Camelino runtime API + HAL 接口（不直接依赖特定平台 SDK）

### 10.3 验证工具

`tools/camelino-check/` — PC 端静态分析：

```bash
camelino-check my_app.cma --heap 192k --flash 512k --word-size 32 --big-endian 0 --platform arduino
# 输出：所需 primitives、所需 HAL capabilities、堆峰值估算、不兼容 API 警告、字长截断风险（§3.4.1.1）、平台能力缺口（§7.6.2）
```

### 10.4 差分测试方法学（验证 ZAM 正确性的唯一可靠手段）

验证解释器正确性的方法：**同一段 `.cmo`/`.cma` 字节码，分别用官方 `ocamlrun` 和 Camelino 执行，逐项比对输出**。OMicroB / OCaPIC 均采用此法。

- 每实现一组指令（P0/P1/P2），就用 `ocamlrun` 跑一批 OCaml testsuite / 手写样例，比对 stdout、退出码、异常、返回值。
- `tools/test_suite/` 提供脚本化对比：对每个样例生成 `.cmo`，分别喂给 `ocamlrun` 和 Camelino 设备/模拟器，输出归一化后 diff。
- **阶段门槛**：Phase 1（算术）、Phase 2（fib + 异常）的验收必须包含差分测试通过，而非"看起来跑对了"。
- 平台可移植性验证：同一字节码在 32 位小端（RP2350）、64 位小端、64 位大端目标上分别差分对比，确保字长/字节序/浮点处理一致（§1.4、§3.4）。

---

## 十一、项目文件结构

```
camelino/
├── LICENSE
├── README.md
├── library.properties
├── keywords.txt
│
├── lib/                          # OCaml 端库（opam/dune）
│   └── camelino/
│       ├── dune
│       ├── camelino.ml           # external 声明
│       └── camelino.mli
│
├── src/                          # 设备端 C 运行时（平台无关）
│   ├── Camelino.h / Camelino.cpp # Arduino 集成层（可选，仅 Arduino port）
│   ├── core/
│   │   ├── value.h / value.c
│   │   ├── memory.h / memory.c
│   │   ├── vm.h / vm.c
│   │   ├── opcodes.h
│   │   └── bytecode.h / bytecode.c
│   ├── ffi/
│   │   ├── ffi.h / ffi.c
│   │   └── primitives.c
│   ├── repl/
│   │   ├── repl.h / repl.c
│   │   ├── line_editor.c
│   │   └── history.c
│   ├── bindings/                 # CAMLprim 包装层（只调用 HAL，§7.6）
│   │   ├── gpio.c
│   │   ├── serial.c
│   │   ├── analog.c
│   │   └── time.c
│   └── hal/                      # 平台无关 HAL 接口（纯声明，§7.6）
│       ├── gpio.h / uart.h / time.h / analog.h / flash.h / interrupt.h / cap.h
│
├── platform/                     # 平台适配器（每个移植目标一个目录）
│   ├── arduino/                  # 首期：Arduino Core（含 arduino-pico 的 RP2350）
│   │   ├── hal_adapter.c
│   │   ├── platform_config.h
│   │   └── port_init.c
│   ├── picosdk/                  # pico-sdk 裸 API（未来）
│   ├── espidf/                   # ESP-IDF（未来）
│   ├── zephyr/                   # Zephyr RTOS（未来）
│   ├── baremetal/                # 裸机（未来）
│   └── host/                     # 主机模拟器（差分测试，§10.4）
│
├── examples/
│   ├── Blink/                    # 预编译字节码点灯
│   ├── REPL/                     # 串口字节码加载
│   └── Sensor/
│
├── tools/
│   ├── camelino-embed/           # .cma → bytecode.h（链接+重定位+字长/字节序重写，§3.2）
│   ├── camelino-check/           # 兼容性静态检查（§10.3）
│   └── test_suite/               # ZAM 指令级 + 差分测试（§10.4）
│
└── test/
    └── test_value.c
```

---

## 十二、开发路线图

### Phase 1：值表示 + 最小 ZAM（第 1–3 周）

- [ ] `value.h` 对齐 OCaml 官方命名（`Val_long` / `Field` / `Hd_val`），低位约定整数=1/指针=0（§5）
- [ ] header 含 GC `Color` 位（§5.0）
- [ ] 堆块分配器（bump allocator，无 GC）
- [ ] `CAML_READ_*` / `CAML_WRITE_*` 字节序显式读写宏（§1.4）
- [ ] 实现 P0 指令子集（含 DIVINT/MODINT 的 `raise_on_overflow` 标志）
- [ ] `camelino-embed` 原型：`.cmo` 解析 + 重定位 + 字长重写（方案 B，§3.4.1.1）
- [ ] **验收：** `ocamlc` 编译的 `2 + 3` 在设备上输出 `5`（含差分测试通过）

### Phase 2：函数 + 控制流（第 4–6 周）

- [ ] 返回帧 / trap 帧布局（§4.5）
- [ ] 4.14 闭包 + closinfo（§5.2），GRAB / APPLY / APPTERM / RESTART 完整实现
- [ ] 全局变量初始化（`GETGLOBAL` / `SETGLOBAL`，拓扑序）
- [ ] 异常（`PUSHTRAP` / `RAISE` / `POPTRAP`）
- [ ] **验收：** `ocamlc` 编译的 `let rec fib n = ...` 正常运行（差分测试）

### Phase 3：GC + 堆块（第 7–9 周）

- [ ] Mark-Sweep GC（header Color 位，§5.0/§6.3）
- [ ] `MAKEBLOCK` / `GETFIELD` / 字符串（§5.3）/ 浮点数组（§5.4，软/硬 FPU 通用）
- [ ] stdlib 基础模块（List, Array）
- [ ] **验收：** 长时间运行无内存泄漏

### Phase 4：FFI + HAL + 首个平台适配器（第 10–11 周）

- [ ] `C_CALLN` primitive dispatch（`external` + `CAMLprim`，§7.1）
- [ ] HAL 接口定义（`hal/gpio.h` 等，§7.6.3）+ 能力探测（§7.6.2）
- [ ] Arduino 适配器（`platform/arduino/hal_adapter.c`，首期 RP2350 via arduino-pico）
- [ ] C → OCaml 回调 `caml_callback`（§7.4）
- [ ] 中断 flag + 主循环轮询模型（§7.5）
- [ ] `lib/camelino/` OCaml 库 + C stubs
- [ ] Blink / Sensor 示例
- [ ] **验收：** OCaml 代码 `digital_write 25 1` 点灯

### Phase 5：完整 ZAM + stdlib（第 12–16 周）

- [ ] P1 + P2 全部指令
- [ ] 调度 yield 模型（§9.4）
- [ ] 浮点、对象、方法
- [ ] Printf → UART 输出
- [ ] `ocamlclean` 集成
- [ ] **验收：** 使用 stdlib 的 List.map / Printf 程序运行

### Phase 6：生态工具 + 多平台移植（第 17–20 周）

- [ ] `camelino-check` 静态分析（§10.3）
- [ ] 差分测试套件全量（§10.4）
- [ ] 主机模拟器 port（`platform/host/`）——差分测试载体
- [ ] 第二个平台适配器（Pico SDK 或 ESP-IDF），验证 HAL 解耦
- [ ] 字节序 / 字长 / 软硬 FPU 多平台差分验证（§1.4、§3.4）
- [ ] dune 规则 / PlatformIO 集成
- [ ] 主机辅助 REPL（`camelino-repl`）
- [ ] 文档 + OPAM 包发布
- [ ] **验收：** 同一份字节码在 ≥2 个平台上差分通过；第三方 OPAM 纯 OCaml 库可移植运行

---

## 十三、关键技术决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 字节码格式 | 标准 OCaml `.cmo` / ZAM | 兼容 `ocamlc` 和 OPAM 生态 |
| 虚拟机 | ZAM（149 指令） | OCaml 官方模型，OMicroB 验证可行 |
| 值表示 | tagged integer（官方 layout，低位 1=整数）| 与 `ocamlrun` 一致，stdlib 兼容 |
| 闭包布局 | 4.14（含 closinfo） | 与目标 OCaml 版本一致，§5.2 |
| GC | Mark-Sweep（header Color 位） | 嵌入式成熟方案，OMicroB 同款 |
| 内存 | 链接器静态池 | 避免 SDK 堆冲突 |
| FFI | `external` + `CAMLprim`（OCaml→C）；`caml_callback`（C→OCaml）| OCaml 标准机制，覆盖事件回调 |
| **硬件访问** | **统一 HAL 接口 + 平台适配器** | 解耦平台 SDK，移植只需实现 `hal_*`，§7.6 |
| 裁剪 | `ocamlclean` | 移除未使用的 stdlib / 库代码 |
| 编译 | 仅 PC 端 | MCU 无法运行 OCaml 编译器 |
| **字长适配** | **方案 B（embed 做 word-size 重写）** | 允许 64 位 host 编译任意字长 target，§3.4.1 |
| OCaml 版本 | 4.14.x（初始） | 稳定，OMicroB 兼容经验 |
| **可移植性** | **字长/字节序/FPU + 平台 SDK 全部参数化/解耦** | 覆盖 32/64 位、大小端、软/硬 FPU、多 SDK，§1.4/§7.6 |
| 调度 | VM yield + 平台主循环协作 | 兼容死循环与一次性程序，平台无关，§9.4 |
| 实时性 | GC 安全点 + 无 ISR 调用 | 嵌入式可靠性，§6.3/§7.5 |
| 验证方法 | 差分测试对 `ocamlrun` | 解释器正确性的唯一可靠手段，§10.4 |

---

## 十四、约束与已知限制

| 限制 | 说明 | 缓解方案 |
|------|------|----------|
| 无原生代码 | MCU 不运行 `ocamlopt` 输出 | 仅使用字节码库 |
| 内存上限 | 板载 SRAM（首期 RP2350 520KB） | `ocamlclean` + 堆大小配置（`platform_config.h`） |
| Flash 上限 | 字节码 + runtime 共享 | 死代码消除，按需链接 stdlib |
| 无文件系统 | 默认无 SD | 字节码嵌入 Flash 或串口加载 |
| 无完整 Sys/Unix | 桌面 API 不可用 | 平台 shim + 编译期检测 |
| GC 暂停 | Stop-the-world | 小堆 + 增量阈值调优 |
| **软 FPU 性能** | 无 FPU 平台浮点慢 1–2 个数量级 | 文档明确；重浮点库默认不推荐；硬 FPU 平台零成本 |
| **字长截断** | 64→32 位重写时，超 31 位 int 无法表示 | embed 工具编译期报错（§3.4.1.1） |
| **平台能力差异** | 不同平台 HAL 能力子集不同（如无 Flash/ADC） | `hal_cap_supported` 能力探测 + 运行时降级（§7.6.2） |
| **性能预算** | ZAM 解释器约 10–50 MIPS，比原生慢 1–2 个数量级 | 适合逻辑控制/状态机/数据处理，不适合高频信号处理/硬实时 |
| 调试 | 无 `ocamldebug` 直接支持 | 主机模拟器（`platform/host/`）+ UART trace + 差分测试 |

---

## 十五、参考资源

- [OCaml Compiler Backend](https://ocaml.org/docs/compiler-backend)
- [ZAM 论文 — Xavier Leroy](https://xavierleroy.org/talks/zam-kazam05.pdf)
- [OMicroB 项目](https://github.com/stevenvar/OMicroB)
- [OCaPIC 项目](https://github.com/bvaugon/ocapic)
- OCaml 源码：`ocaml/bytecomp/instruct.ml`，`ocaml/runtime/` 
- `.cmo` 格式：`ocaml/file_formats/cmo_format.ml`
- 嵌入式 HAL 设计参考：Zephyr Device Driver Model、RP2040 HAL、Arduino Core API

---

*本文档随项目演进更新。当前版本 0.4.0：在 0.3.0（OCaml 标准字节码兼容、字长/字节序/软硬 FPU 参数化、方案 B word-size 重写、4.14 闭包布局、`caml_callback` 回调、VM yield 调度、差分测试）基础上，**引入统一硬件抽象层（HAL）解耦平台 SDK**——核心运行时零 SDK 依赖，硬件/时序/IO 经 `hal_*` 接口暴露，各平台提供适配器实现，支持 Arduino / Pico SDK / ESP-IDF / Zephyr / 裸机 / 主机模拟器多平台移植。*
