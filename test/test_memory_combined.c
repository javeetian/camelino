/**
 * @file test/test_memory_combined.c
 * @brief 单文件编译入口 — #include 所有依赖源文件
 *
 * 解决 MinGW GCC 14.2 + Ninja 多 .o 链接时 collect2 的奇怪错误（exit code 1/5）。
 * 此文件将 test + 所有 core 源文件编译为一个翻译单元，绕过链接器问题。
 * 当 MinGW 升级或迁移到 Linux CI 后可移除此文件，改回 add_executable 多源。
 */
#include "test_memory.c"
#include "../src/core/value.c"
#include "../src/core/memory.c"
#include "../src/core/error.c"
