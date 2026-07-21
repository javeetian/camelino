/**
 * @file platform/host/platform_config.h
 * @brief 主机模拟器平台配置（差分测试用）
 *
 * x86-64 / ARM64: 64-bit little-endian, hardware FPU
 */

#ifndef CAMELINO_PLATFORM_CONFIG_H
#define CAMELINO_PLATFORM_CONFIG_H

#define CAMELINO_WORD_SIZE   64
#define CAMELINO_BIG_ENDIAN  0
#define CAMELINO_HAS_FPU     1

#define HEAP_SIZE      (16 * 1024 * 1024)   /* 16MB，主机端宽松 */
#define STACK_SIZE     65536
#define MAX_GLOBALS    65536

#endif
