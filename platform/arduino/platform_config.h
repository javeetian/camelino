/**
 * @file platform/arduino/platform_config.h
 * @brief Arduino/RP2350 平台配置（首期目标）
 *
 * RP2350: 32-bit little-endian, Cortex-M33 (no hardware FPU)
 */

#ifndef CAMELINO_PLATFORM_CONFIG_H
#define CAMELINO_PLATFORM_CONFIG_H

/* ---- §1.4 可移植性宏 ---- */
#define CAMELINO_WORD_SIZE   32
#define CAMELINO_BIG_ENDIAN  0
#define CAMELINO_HAS_FPU     0   /* 软 FPU */

/* ---- §6.2 内存池（RP2350 520KB SRAM）---- */
#define HEAP_SIZE      (192 * 1024)
#define STACK_SIZE     8192
#define MAX_GLOBALS    4096

#endif /* CAMELINO_PLATFORM_CONFIG_H */
