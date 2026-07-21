/**
 * @file hal/interrupt.h
 * @brief HAL 中断抽象接口 — ISR 只置 flag，不进 VM
 *
 * ISR 中仅允许设置 volatile flag。
 * 主循环在安全点轮询 flag → caml_callback 触发 OCaml 处理函数（§7.5、§9.4）。
 */

#ifndef CAMELINO_HAL_INTERRUPT_H
#define CAMELINO_HAL_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/err.h"

typedef enum {
    HAL_GPIO_EDGE_RISING,
    HAL_GPIO_EDGE_FALLING,
    HAL_GPIO_EDGE_BOTH
} hal_gpio_edge_t;

typedef volatile uint32_t hal_isr_flag_t;

hal_err_t hal_gpio_attach_isr(uint32_t pin, hal_gpio_edge_t edge,
                              hal_isr_flag_t* flag);
hal_err_t hal_gpio_detach_isr(uint32_t pin);
bool      hal_isr_flag_take(hal_isr_flag_t* flag); /* 原子取走并清零 */

#endif
