# Camelino — CMake 构建系统
# ============================
#
# 两个目标：
#   camelino-host  — 主机模拟器（差分测试用）
#   camelino-test  — 设备端 C 单元测试（在主机上运行）
#
# 交叉编译（arm-none-eabi-gcc）在 CMake toolchain file 中配置，
# 由 PlatformIO / CI 脚本驱动。

cmake_minimum_required(VERSION 3.16)
project(Camelino VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- 编译选项 ----

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# ---- 平台选择 ----
# 默认 host；交叉编译时由 toolchain file 覆盖
if(NOT DEFINED CAMELINO_PLATFORM)
    set(CAMELINO_PLATFORM "host" CACHE STRING "Target platform: host, arduino, picosdk, espidf, zephyr, baremetal")
endif()

# ---- 根目录（用于 include path） ----
set(CAMELINO_ROOT ${CMAKE_SOURCE_DIR})

# ---- 平台配置 ----
# 各平台在 platform/<name>/platform_config.h 中定义 CAMELINO_WORD_SIZE 等宏
# 构建系统只需添加正确的 include 路径
if(CAMELINO_PLATFORM STREQUAL "host")
    add_definitions(-DCAMELINO_PLATFORM_HOST)
    set(PLATFORM_DIR ${CAMELINO_ROOT}/platform/host)
elseif(CAMELINO_PLATFORM STREQUAL "arduino")
    add_definitions(-DCAMELINO_PLATFORM_ARDUINO)
    set(PLATFORM_DIR ${CAMELINO_ROOT}/platform/arduino)
else()
    message(FATAL_ERROR "Unknown CAMELINO_PLATFORM: ${CAMELINO_PLATFORM}")
endif()

# ---- 公共 include 路径 ----
# 顺序重要：platform 目录在前，这样 platform_config.h 优先被找到
set(COMMON_INCLUDES
    ${PLATFORM_DIR}            # platform_config.h
    ${CAMELINO_ROOT}/src       # camelino core headers
    ${CAMELINO_ROOT}/src/core  # value.h, memory.h, vm.h, ...
    ${CAMELINO_ROOT}/src/hal   # hal/*.h
    ${CAMELINO_ROOT}/src/ffi   # ffi.h
    ${CAMELINO_ROOT}/src/bindings
    ${CAMELINO_ROOT}/src/repl
)

# ---- 公共源文件（平台无关） ----

set(CORE_SOURCES
    src/core/value.c
    src/core/memory.c
    src/core/error.c
    src/core/vm.c
    src/core/bytecode.c
)

set(FFI_SOURCES
    src/ffi/ffi.c
    src/ffi/primitives.c
)

set(REPL_SOURCES
    src/repl/repl.c
    src/repl/line_editor.c
    src/repl/history.c
)

set(BINDINGS_SOURCES
    src/bindings/gpio.c
    src/bindings/serial.c
    src/bindings/analog.c
    src/bindings/time.c
    src/bindings/error.c
)

# ---- camelino-host：主机模拟器 ----

if(CAMELINO_PLATFORM STREQUAL "host")
    # camelino-host：wrapper.c #include 所有实现（规避 MinGW collect2 问题）
    add_custom_target(camelino_host_target  # optional, not in default (MinGW collect2)
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CAMELINO_ROOT}/build/host_wrapper.c
            host_wrapper.c
        COMMAND gcc host_wrapper.c
            -I${PLATFORM_DIR}
            -I${CAMELINO_ROOT}/src
            -I${CAMELINO_ROOT}/src/core
            -std=gnu99 -Wall
            -o camelino-host.exe
        COMMENT "Building camelino-host (wrapper + gcc single TU)"
    )
endif()

# ---- camelino-test：单元测试 ----

# 每个 .c 文件中的 test_* 函数由 test runner 收集
# 简单起见，直接用 CTest 跑单独编译的测试二进制
enable_testing()

# test_value
add_executable(test_value
    test/test_value.c
    src/core/value.c
)
target_include_directories(test_value PRIVATE
    ${PLATFORM_DIR}
    ${CAMELINO_ROOT}/src/core
)
target_compile_definitions(test_value PRIVATE -DCAMELINO_UNIT_TEST)
add_test(NAME test_value COMMAND test_value)

# test_opcodes
add_executable(test_opcodes
    test/test_opcodes.c
)
target_include_directories(test_opcodes PRIVATE
    ${PLATFORM_DIR}
    ${CAMELINO_ROOT}/src/core
)
add_test(NAME test_opcodes COMMAND test_opcodes)

# test_memory — 单步 cat + 编译（规避 MinGW GCC 14.2 多 .o 链接 bug）
add_custom_target(test_memory_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_memory.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_memory_combined.c
    COMMAND gcc test_memory_combined.c
        -I${PLATFORM_DIR}
        -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall
        -o test_memory.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_memory_combined.c
    DEPENDS
        ${CAMELINO_ROOT}/test/test_memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_memory (cat+compile single TU)"
)
add_test(NAME test_memory COMMAND test_memory.exe)

# test_vm
add_custom_target(test_vm_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_vm.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_vm_combined.c
    COMMAND gcc test_vm_combined.c
        -I${PLATFORM_DIR}
        -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable
        -o test_vm.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_vm_combined.c
    DEPENDS
        ${CAMELINO_ROOT}/test/test_vm.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_vm (cat+compile single TU)"
)
add_test(NAME test_vm COMMAND test_vm.exe)

# test_bytecode
add_custom_target(test_bytecode_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_bytecode.c
        ${CAMELINO_ROOT}/src/core/bytecode.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_bytecode_combined.c
    COMMAND gcc test_bytecode_combined.c
        -I${PLATFORM_DIR}
        -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable
        -o test_bytecode.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_bytecode_combined.c
    DEPENDS
        ${CAMELINO_ROOT}/test/test_bytecode.c
        ${CAMELINO_ROOT}/src/core/bytecode.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_bytecode (cat+compile single TU)"
)
add_test(NAME test_bytecode COMMAND test_bytecode.exe)

# test_embed
add_custom_target(test_embed_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_embed.c
        ${CAMELINO_ROOT}/src/core/bytecode.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_embed_combined.c
    COMMAND gcc test_embed_combined.c
        -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable
        -o test_embed.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_embed_combined.c
    DEPENDS
        ${CAMELINO_ROOT}/test/test_embed.c
        ${CAMELINO_ROOT}/src/core/bytecode.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_embed"
)
add_test(NAME test_embed COMMAND test_embed.exe)

# test_calls
add_custom_target(test_calls_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_calls.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_calls_combined.c
    COMMAND gcc test_calls_combined.c
        -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable
        -o test_calls.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_calls_combined.c
    DEPENDS
        ${CAMELINO_ROOT}/test/test_calls.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_calls"
)
add_test(NAME test_calls COMMAND test_calls.exe)

# test_exn
add_custom_target(test_exn_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_exn.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_exn_combined.c
    COMMAND gcc test_exn_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable -o test_exn.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_exn_combined.c
    DEPENDS ${CAMELINO_ROOT}/test/test_exn.c
        ${CAMELINO_ROOT}/src/core/vm.c ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c ${CAMELINO_ROOT}/src/core/error.c
    COMMENT "Building test_exn"
)
add_test(NAME test_exn COMMAND test_exn.exe)

# test_globals
add_custom_target(test_globals_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_globals.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_globals_combined.c
    COMMAND gcc test_globals_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable -o test_globals.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_globals_combined.c
    COMMENT "Building test_globals"
)
add_test(NAME test_globals COMMAND test_globals.exe)

# test_gc
add_custom_target(test_gc_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_gc.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_gc_combined.c
    COMMAND gcc test_gc_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable -Wno-unused-function
        -o test_gc.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_gc_combined.c
    COMMENT "Building test_gc"
)
add_test(NAME test_gc COMMAND test_gc.exe)

# test_ffi
add_custom_target(test_ffi_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_ffi.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        ${CAMELINO_ROOT}/src/ffi/ffi.c
        > test_ffi_combined.c
    COMMAND gcc test_ffi_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core -I${CAMELINO_ROOT}/src/ffi
        -DCAMELINO_HAS_FFI -std=gnu99 -Wall -Wno-unused-variable -o test_ffi.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_ffi_combined.c
    COMMENT "Building test_ffi"
)
add_test(NAME test_ffi COMMAND test_ffi.exe)

# test_phase4 (4.4-4.10 integration)
add_custom_target(test_phase4_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_phase4.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        ${CAMELINO_ROOT}/src/ffi/ffi.c
        > test_phase4_combined.c
    COMMAND gcc test_phase4_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core -I${CAMELINO_ROOT}/src/ffi
        -DCAMELINO_HAS_FFI -std=gnu99 -Wall -Wno-unused-variable -o test_phase4.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_phase4_combined.c
    COMMENT "Building test_phase4"
)
add_test(NAME test_phase4 COMMAND test_phase4.exe)

# test_phase5
add_custom_target(test_phase5_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_phase5.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        ${CAMELINO_ROOT}/src/ffi/ffi.c
        > test_phase5_combined.c
    COMMAND gcc test_phase5_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core -I${CAMELINO_ROOT}/src/ffi
        -DCAMELINO_HAS_FFI -std=gnu99 -Wall -Wno-unused-variable -o test_phase5.exe
        -std=gnu99 -Wall -Wno-unused-variable -o test_phase5.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_phase5_combined.c
    COMMENT "Building test_phase5"
)
add_test(NAME test_phase5 COMMAND test_phase5.exe)

# test_gc_roots
add_custom_target(test_gc_roots_target ALL
    COMMAND ${CMAKE_COMMAND} -E cat
        ${CAMELINO_ROOT}/test/test_gc_roots.c
        ${CAMELINO_ROOT}/src/core/vm.c
        ${CAMELINO_ROOT}/src/core/memory.c
        ${CAMELINO_ROOT}/src/core/value.c
        ${CAMELINO_ROOT}/src/core/error.c
        ${CAMELINO_ROOT}/src/core/gc_roots.c
        > test_gc_roots_combined.c
    COMMAND gcc test_gc_roots_combined.c -I${PLATFORM_DIR} -I${CAMELINO_ROOT}/src/core
        -std=gnu99 -Wall -Wno-unused-variable -o test_gc_roots.exe
    COMMAND ${CMAKE_COMMAND} -E remove -f test_gc_roots_combined.c
    COMMENT "Building test_gc_roots"
)
add_test(NAME test_gc_roots COMMAND test_gc_roots.exe)
