/**
 * @file hal/analog.h
 * @brief HAL 模拟 I/O 抽象接口 — ADC & PWM
 */

#ifndef CAMELINO_HAL_ANALOG_H
#define CAMELINO_HAL_ANALOG_H

#include <stdint.h>
#include "hal/err.h"

#define HAL_PWM_MAX 255

hal_err_t hal_adc_init(uint32_t pin);
uint32_t  hal_adc_read(uint32_t pin);           /* 原始值，分辨率由平台配置 */
int       hal_adc_resolution(void);             /* 返回 ADC 位数 (8/10/12/16) */

hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz);
hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty); /* [0, HAL_PWM_MAX] */

#endif
