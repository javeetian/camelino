/**
 * @file hal/time.h
 * @brief HAL 时序抽象接口 — 平台无关的延迟与时间戳
 */

#ifndef CAMELINO_HAL_TIME_H
#define CAMELINO_HAL_TIME_H

#include <stdint.h>

uint64_t hal_time_millis(void);
uint64_t hal_time_micros(void);
void     hal_delay_ms(uint32_t ms);
void     hal_delay_us(uint32_t us);

#endif
