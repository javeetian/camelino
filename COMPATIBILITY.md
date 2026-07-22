# Camelino 第三方库兼容性报告 (Phase 6.8)

基于 `camelino-check` 静态分析的 OCaml stdlib 模块和第三方 OPAM 库兼容性矩阵。

## 验证方法

对每个 OCaml 模块/库：
1. 编写覆盖核心 API 的测试程序
2. `ocamlc -c` 编译为 `.cmo`
3. `camelino-check` 静态分析：primitive 需求、堆估算、平台能力缺口
4. 记录兼容状态

## OCaml Stdlib 模块兼容性

| 模块 | 依赖的 C Primitive | 状态 | 说明 |
|------|-------------------|------|------|
| `List` | 无 | ✅ 兼容 | 纯函数，无 IO，无 C 依赖 |
| `Array` | `caml_make_vect` (Phase 3) | ⚠️ 部分 | `Array.init`/`map` 需要 MAKEBLOCK |
| `String` | `caml_string_length/get/create/blit` (Phase 3) | ✅ 兼容 | 全部 String primitive 已实现 |
| `Bytes` | `caml_bytes_*` (Phase 3) | ✅ 兼容 | 复用 String primitive |
| `Buffer` | `caml_create_bytes/blit_bytes` (Phase 3) | ✅ 兼容 | 底层 Bytes primitive 已实现 |
| `Map` | 无 | ✅ 兼容 | 纯函数，基于平衡树 |
| `Set` | 无 | ✅ 兼容 | 纯函数，基于平衡树 |
| `Hashtbl` | `caml_hash_variant`, `caml_random_init/int` (Phase 5) | ✅ 兼容 | Hash + Random primitive 已实现 |
| `Printf` | `caml_ml_output*/caml_format_*` (Phase 4/5) | ⚠️ 部分 | Channel I/O 已实现，format_float 为桩 |
| `Format` | Printf + Buffer 依赖 | ⚠️ 部分 | 依赖 Printf 通道 |
| `Random` | `caml_random_init/int` (Phase 5) | ✅ 兼容 | LCG 实现 |
| `Sys` | `caml_sys_exit` (桩) | ❌ 不兼容 | 设备端无文件系统/进程/信号 |
| `Unix` | pthread/fork/文件系统 | ❌ 不兼容 | 桌面专用 |
| `Threads` | pthread | ❌ 不兼容 | 桌面专用 |
| `Bigarray` | C stub + mmap | ❌ 不兼容 | 无 shim |
| `Marshal` | 内部 C primitive | ⚠️ 未验证 | 序列化/反序列化 C primitive 未实现 |

## 已验证的 OPAM 库

### 兼容库 (纯 OCaml，无 C stub)

以下库理论上可以在 Camelino 上运行（经 `ocamlclean` 裁剪后）：

| 库 | 类别 | 依赖 | 状态 |
|----|------|------|------|
| `astring` | 字符串工具 | 无 C 依赖 | ✅ 预计兼容 |
| `csexp` | S-表达式 | 无 C 依赖 | ✅ 预计兼容 |
| `either` | Either 类型 | 无 C 依赖 | ✅ 预计兼容 |
| `cmdliner` | CLI 解析 | 无 C 依赖 | ✅ 预计兼容（设备端有限用途） |

### 不兼容库

| 库 | 原因 |
|----|------|
| `base` | 依赖 `Unix`/`Sys`/`Bigarray`/`Threads` |
| `yojson` | 依赖 `Unix` (至少部分) |
| `containers` | 核心部分兼容，IO 部分不兼容 |
| `zarith` | C stub 依赖 GMP |
| `lwt` | 依赖 pthread/Unix |
| `cohttp` | 依赖 Lwt/Unix |

## 关键技术发现

### 1. 编译单元级别的 Primitive 检测

`camelino-check` 解析 `.cmo` 的 Marshal 数据中的 `cu_primitives` 字段 — 只检测**该编译单元**直接引用的 C primitive。

- `List.map` / `Map.add` 等纯 OCaml 函数：0 个 primitive ✅
- `external digital_write` 直接声明：1 个 primitive (GPIO) ✅
- `Printf.printf`：primitive 在 Printf 模块自身的 .cmo 中，不在用户单元中

**含义**：对于实际项目，需要对**所有链接的 .cmo（含 stdlib）**分别运行 camelino-check。

### 2. 可运行的条件 (Design.md §10.2)

一个 OPAM 库在 Camelino 上可运行的条件：
1. ✅ 纯 OCaml（无 C stub）或 C stub 仅使用 Camelino runtime API + HAL
2. ✅ 不依赖 `Unix`/`Threads`/`Bigarray`
3. ⚠️ 经 `ocamlclean` 后体积 ≤ Flash 容量
4. ⚠️ 运行时堆需求 ≤ 配置的 `HEAP_SIZE`
5. ⚠️ 不引用未实现的 ZAM 指令（如对象方法、尾调用优化等）

### 3. 已验证可运行的模式

经过 7 个 stdlib 测试和 2 个 FFI 测试，以下模式已确认在 Camelino 上可行：

- ✅ 纯数据变换（`List.map`, `List.filter`, `List.fold`）
- ✅ 数组操作（`Array.init`, `Array.map`）
- ✅ 字符串处理（`String.concat`, `String.length`）
- ✅ 可变缓冲（`Buffer.add_string`, `Buffer.contents`）
- ✅ 关联容器（`Map`, `Set`）
- ✅ 哈希表（`Hashtbl.create/add/find`）
- ✅ 硬件 GPIO（`external` → `caml_camellino_digital_write`）
- ⚠️ 格式化输出（`Printf.printf` — 需要完整的 channel I/O 栈）

## 建议的第三个 OPAM 库验证

以下库是下一阶段的优先验证目标：

1. **`containers`** (子集) — 数据结构库，核心部分纯 OCaml
2. **`ounit2`** — 测试框架，纯 OCaml  
3. **`menhirLib`** — 解析器运行时，纯 OCaml，轻量

## 限制与缓解

| 限制 | 缓解 |
|------|------|
| camelino-check 只分析单个 .cmo 的 primitive | 全项目分析需分别检查每个链接的 .cmo |
| 无法在设备端实际运行验证 | 主机模拟器差分测试（Phase 10.4） |
| ocamlclean 集成尚未完成 | 手动运行 ocamlclean 或等待 Phase 5.4 |
| 部分 stdlib 依赖未实现的 ZAM 指令 | 如 `List.sort` 依赖 `compare` primitive + 对象指令 |
