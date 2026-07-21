/**
 * @file hal/gpio.h
 * @brief HAL GPIO 抽象接口 — 平台无关的数字 I/O
 *
 * 每个平台在 platform/<name>/hal_adapter.c 中实现此接口。
 * 调用者（bindings/*.c）不直接依赖任何平台 SDK（如 Arduino digitalWrite）。
 */

#ifndef CAMELINO_HAL_GPIO_H
#define CAMELINO_HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/err.h"

typedef enum {
    HAL_GPIO_INPUT,
    HAL_GPIO_INPUT_PULLUP,
    HAL_GPIO_INPUT_PULLDOWN,
    HAL_GPIO_OUTPUT,
    HAL_GPIO_OUTPUT_OD
} hal_gpio_mode_t;

hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode);
hal_err_t hal_gpio_write(uint32_t pin, bool value);
bool      hal_gpio_read(uint32_t pin);
hal_err_t hal_gpio_toggle(uint32_t pin);

#endif
