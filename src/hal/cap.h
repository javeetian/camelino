/**
 * @file hal/cap.h
 * @brief HAL 能力探测 — 运行时查询平台支持的能力子集
 *
 * 不同平台能力子集不同。未实现的 capability 在 OCaml 侧抛 Failure，
 * 而非链接期失败——同一份字节码可在不同能力的平台上降级运行。
 */

#ifndef CAMELINO_HAL_CAP_H
#define CAMELINO_HAL_CAP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HAL_CAP_GPIO           = 0x0001,
    HAL_CAP_UART           = 0x0002,
    HAL_CAP_ADC            = 0x0004,
    HAL_CAP_PWM            = 0x0008,
    HAL_CAP_FLASH          = 0x0010,
    HAL_CAP_GPIO_INTERRUPT = 0x0020,
    HAL_CAP_TIME_MICROS    = 0x0040,
    HAL_CAP_SLEEP          = 0x0080,
    HAL_CAP_I2C            = 0x0100,   /* 未来 */
    HAL_CAP_SPI            = 0x0200    /* 未来 */
} hal_cap_id_t;

bool hal_cap_supported(hal_cap_id_t cap);

#endif
