/**
 * @file value.h
 * @brief OCaml 值的 C 语言表示系统
 * 
 * 这是 Camelino 虚拟机最基础的模块，定义了 OCaml 值在内存中的表示方式。
 * 
 * 核心设计：
 * - 每个值是一个指针大小的整数 (uintptr_t)
 * - 最低位（LSB）是标记位：0 表示整数，1 表示指针
 * - 31/63 位整数（移1位存储），剩余位用于指针
 * 
 * 内存布局：
 *   value (32位): [31................1][0]
 *                                    ↑
 *                                    tag (0=整数,1=指针)
 * 
 *   value (64位): [63................1][0]
 *                                    ↑
 *                                    tag
 * 
 * @copyright MIT License
 * @author Camelino Contributors
 */

 #ifndef CAMELINO_VALUE_H
 #define CAMELINO_VALUE_H
 
 #include <stdint.h>
 #include <stddef.h>
 #include <stdbool.h>
 
 /*==============================================================================
  * 基础类型定义
  *============================================================================*/
 
 /**
  * @brief OCaml 值的核心类型
  * 
  * 一个 caml_value_t 可以表示：
  * - 整数（最低位为 0）
  * - 指向堆块的指针（最低位为 1）
  */
 typedef uintptr_t caml_value_t;
 
 /**
  * @brief 堆块的头部（32位）
  * 
  * 高22位：块大小（字段数，0 到 4,194,303）
  * 低10位：标签（0 到 1023）
  */
 typedef uint32_t caml_header_t;
 
 
 /*==============================================================================
  * 标记位常量
  *============================================================================*/
 
 /** 整数标记位（最低位为 0） */
 #define CAML_TAG_INT      0
 
 /** 指针标记位（最低位为 1） */
 #define CAML_TAG_PTR      1
 
 /** 用于提取标记位的掩码 */
 #define CAML_TAG_MASK     1
 
 
 /*==============================================================================
  * 整数编码/解码
  *============================================================================*/
 
 /**
  * @brief 将 C 整数编码为 OCaml 值
  * @param x C 整数（范围：-2^30 到 2^30-1）
  * @return 编码后的 OCaml 值
  * 
  * 示例：
  *   Caml_val_int(0)   -> 0
  *   Caml_val_int(1)   -> 2
  *   Caml_val_int(-1)  -> -2
  */
 static inline caml_value_t Caml_val_int(intptr_t x) {
     return ((caml_value_t)x << 1) | CAML_TAG_INT;
 }
 
 /**
  * @brief 将 OCaml 整数解码为 C 整数
  * @param v OCaml 值（必须是一个整数）
  * @return 解码后的 C 整数
  * 
  * 示例：
  *   Caml_int_val(0)   -> 0
  *   Caml_int_val(2)   -> 1
  *   Caml_int_val(-2)  -> -1
  */
 static inline intptr_t Caml_int_val(caml_value_t v) {
     return (intptr_t)(v >> 1);
 }
 
 /**
  * @brief 检查 OCaml 值是否为整数
  * @param v OCaml 值
  * @return true 如果是整数，false 如果是指针
  */
 static inline bool Caml_is_int(caml_value_t v) {
     return ((v & CAML_TAG_MASK) == CAML_TAG_INT);
 }
 
 
 /*==============================================================================
  * 指针编码/解码
  *============================================================================*/
 
 /**
  * @brief 将 C 指针编码为 OCaml 值
  * @param p C 指针（必须按2字节对齐）
  * @return 编码后的 OCaml 值
  * 
  * 注意：指针必须保证最低位为 0，因为我们会用最低位存储标记。
  *       malloc 返回的指针通常满足这个条件。
  */
 static inline caml_value_t Caml_val_ptr(void* p) {
     return ((caml_value_t)p) | CAML_TAG_PTR;
 }
 
 /**
  * @brief 将 OCaml 值解码为 C 指针
  * @param v OCaml 值（必须是一个指针）
  * @return 解码后的 C 指针
  */
 static inline void* Caml_ptr_val(caml_value_t v) {
     return (void*)(v & ~CAML_TAG_MASK);
 }
 
 /**
  * @brief 检查 OCaml 值是否为指针
  * @param v OCaml 值
  * @return true 如果是指针，false 如果是整数
  */
 static inline bool Caml_is_ptr(caml_value_t v) {
     return ((v & CAML_TAG_MASK) == CAML_TAG_PTR);
 }
 
 
 /*==============================================================================
  * 堆块操作
  *============================================================================*/
 
 /**
  * @brief 创建堆块头部
  * @param tag 块标签（0-1023，用于区分不同类型的块）
  * @param size 块中的字段数（0 到 4,194,303）
  * @return 编码后的头部值
  * 
  * 预定义标签：
  *   - 0: 闭包 (Closure)
  *   - 1: 字符串 (String)
  *   - 2: 元组 (Tuple)
  *   - 3: 数组 (Array)
  *   - 4: 异常 (Exception)
  *   - 5-1023: 用户自定义
  */
 static inline caml_header_t Caml_make_header(unsigned int tag, size_t size) {
     return ((caml_header_t)size << 10) | (tag & 0x3FF);
 }
 
 /**
  * @brief 从头部获取块大小
  * @param h 头部值
  * @return 块中的字段数
  */
 static inline size_t Caml_header_size(caml_header_t h) {
     return (size_t)(h >> 10);
 }
 
 /**
  * @brief 从头部获取块标签
  * @param h 头部值
  * @return 块标签（0-1023）
  */
 static inline unsigned int Caml_header_tag(caml_header_t h) {
     return (unsigned int)(h & 0x3FF);
 }
 
 /**
  * @brief 获取块的第 i 个字段
  * @param v 指向块的指针值
  * @param i 字段索引（0 到 size-1）
  * @return 字段值
  */
 static inline caml_value_t Caml_field(caml_value_t v, size_t i) {
     caml_value_t* block = (caml_value_t*)Caml_ptr_val(v);
     return block[i];
 }
 
 /**
  * @brief 设置块的第 i 个字段
  * @param v 指向块的指针值
  * @param i 字段索引（0 到 size-1）
  * @param val 要设置的值
  */
 static inline void Caml_set_field(caml_value_t v, size_t i, caml_value_t val) {
     caml_value_t* block = (caml_value_t*)Caml_ptr_val(v);
     block[i] = val;
 }
 
 
 /*==============================================================================
  * 常用常量
  *============================================================================*/
 
 /** OCaml 的 unit 值（类型为 ()，表示无返回值） */
 #define CAML_UNIT (Caml_val_int(0))
 
 /** OCaml 的 true 值 */
 #define CAML_TRUE (Caml_val_int(1))
 
 /** OCaml 的 false 值 */
 #define CAML_FALSE (Caml_val_int(0))
 
 /** OCaml 的 0 值 */
 #define CAML_ZERO (Caml_val_int(0))
 
 
 /*==============================================================================
  * 预定义标签常量
  *============================================================================*/
 
 /** 闭包标签（一个闭包是一个代码指针 + 环境） */
 #define CAML_TAG_CLOSURE    0
 
 /** 字符串标签（字节序列，不是 OCaml 值数组） */
 #define CAML_TAG_STRING     1
 
 /** 元组标签（固定长度的值数组） */
 #define CAML_TAG_TUPLE      2
 
 /** 数组标签（可变长度的值数组） */
 #define CAML_TAG_ARRAY      3
 
 /** 异常标签 */
 #define CAML_TAG_EXCEPTION  4
 
 /** 抽象标签（不透明的 C 数据结构） */
 #define CAML_TAG_ABSTRACT   5
 
 /** 外部函数标签（FFI 调用的 C 函数） */
 #define CAML_TAG_EXTERNAL   6
 
 /** 最大用户自定义标签值 */
 #define CAML_TAG_MAX_USER   1023
 
 
 /*==============================================================================
  * 辅助宏（用于简化常见操作）
  *============================================================================*/
 
 /**
  * @def Caml_alloc_block(tag, size)
  * @brief 分配一个新的堆块
  * @param tag 块标签
  * @param size 块大小（字段数）
  * @return 指向新块的 caml_value_t
  * 
  * 注意：这个宏需要配合内存分配器使用，这里是声明，
  *       实际实现在 memory.c 中。
  */
 #define Caml_alloc_block(tag, size) \
     caml_alloc_block_internal((tag), (size))
 
 /**
  * @def Caml_alloc_tuple(size)
  * @brief 分配一个元组
  */
 #define Caml_alloc_tuple(size) \
     Caml_alloc_block(CAML_TAG_TUPLE, (size))
 
 /**
  * @def Caml_alloc_array(size)
  * @brief 分配一个数组
  */
 #define Caml_alloc_array(size) \
     Caml_alloc_block(CAML_TAG_ARRAY, (size))
 
 /**
  * @def Caml_alloc_string(length)
  * @brief 分配一个字符串
  * @param length 字节数（不是字段数）
  */
 #define Caml_alloc_string(length) \
     caml_alloc_string_internal((length))
 
 
 /*==============================================================================
  * 内部函数声明（由 memory.c 实现）
  *============================================================================*/
 
 /**
  * @brief 内部：分配堆块
  * @param tag 块标签
  * @param size 字段数
  * @return 指向新块的 caml_value_t
  * 
  * 这个函数在 memory.c 中实现。
  */
 caml_value_t caml_alloc_block_internal(unsigned int tag, size_t size);
 
 /**
  * @brief 内部：分配字符串
  * @param length 字节数
  * @return 指向新字符串的 caml_value_t
  */
 caml_value_t caml_alloc_string_internal(size_t length);
 
 
 /*==============================================================================
  * 类型转换辅助函数
  *============================================================================*/
 
 /**
  * @brief 将 C 布尔值转换为 OCaml 布尔值
  * @param b C 布尔值
  * @return OCaml 布尔值（true 或 false）
  */
 static inline caml_value_t Caml_val_bool(bool b) {
     return b ? CAML_TRUE : CAML_FALSE;
 }
 
 /**
  * @brief 将 OCaml 布尔值转换为 C 布尔值
  * @param v OCaml 布尔值
  * @return C 布尔值
  */
 static inline bool Caml_bool_val(caml_value_t v) {
     return (Caml_int_val(v) != 0);
 }
 
 /**
  * @brief 将 C 字符转换为 OCaml 整数
  * @param c C 字符
  * @return OCaml 整数
  */
 static inline caml_value_t Caml_val_char(char c) {
     return Caml_val_int((intptr_t)c);
 }
 
 /**
  * @brief 将 OCaml 整数转换为 C 字符
  * @param v OCaml 整数
  * @return C 字符
  */
 static inline char Caml_char_val(caml_value_t v) {
     return (char)Caml_int_val(v);
 }
 
 
 /*==============================================================================
  * 调试辅助函数
  *============================================================================*/
 
 /**
  * @brief 将 OCaml 值转换为人类可读的字符串（用于调试）
  * @param v OCaml 值
  * @param out 输出缓冲区
  * @param out_size 缓冲区大小
  * 
  * 注意：这个函数会递归展开堆块，输出格式类似 OCaml 的语法。
  */
 void Caml_debug_print(caml_value_t v, char* out, size_t out_size);
 
 /**
  * @brief 将 OCaml 值打印到串口（用于嵌入式调试）
  * @param v OCaml 值
  * 
  * 这个函数直接通过串口输出，不分配内存。
  */
 void Caml_debug_print_uart(caml_value_t v);
 
 #endif /* CAMELINO_VALUE_H */