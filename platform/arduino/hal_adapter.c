/**
 * @file platform/arduino/hal_adapter.c
 * @brief Arduino HAL 适配器 — 把 hal_* 映射到 Arduino API
 *
 * 首期目标：RP2350 via arduino-pico
 */

#include "hal/err.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "hal/time.h"
#include "hal/analog.h"
#include "hal/interrupt.h"
#include "hal/cap.h"
#include <Arduino.h>

/* ---- GPIO ---- */

hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode) {
    (void)mode;
    /* TODO: 实现 pinMode 映射 */
    return HAL_OK;
}

hal_err_t hal_gpio_write(uint32_t pin, bool value) {
    (void)pin; (void)value;
    /* TODO: 实现 digitalWrite 映射 */
    return HAL_OK;
}

bool hal_gpio_read(uint32_t pin) {
    (void)pin;
    /* TODO: 实现 digitalRead 映射 */
    return false;
}

hal_err_t hal_gpio_toggle(uint32_t pin) {
    (void)pin;
    /* TODO: 实现 digitalWrite(!digitalRead(pin)) */
    return HAL_OK;
}

/* ---- UART ---- */

hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg) {
    (void)port; (void)cfg;
    /* TODO: 实现 Serial.begin 映射 */
    return HAL_OK;
}

int hal_uart_write(uint8_t port, const uint8_t* data, size_t len) {
    (void)port; (void)data; (void)len;
    /* TODO: 实现 Serial.write 映射 */
    return (int)len;
}

int hal_uart_read(uint8_t port, uint8_t* buf, size_t len) {
    (void)port; (void)buf; (void)len;
    /* TODO: 实现 Serial.read 映射（非阻塞） */
    return 0;
}

void hal_uart_flush(uint8_t port) {
    (void)port;
    /* TODO: 实现 Serial.flush */
}

/* ---- Time ---- */

uint64_t hal_time_millis(void) {
    /* TODO: 实现 millis() 映射 */
    return 0;
}

uint64_t hal_time_micros(void) {
    /* TODO: 实现 micros() 映射 */
    return 0;
}

void hal_delay_ms(uint32_t ms) {
    (void)ms;
    /* TODO: 实现 delay(ms) 映射 */
}

void hal_delay_us(uint32_t us) {
    (void)us;
    /* TODO: 实现 delayMicroseconds(us) 映射 */
}

/* ---- Analog ---- */

hal_err_t hal_adc_init(uint32_t pin) {
    (void)pin;
    return HAL_OK;
}

uint32_t hal_adc_read(uint32_t pin) {
    (void)pin;
    /* TODO: 实现 analogRead 映射 */
    return 0;
}

int hal_adc_resolution(void) {
    return 10; /* Arduino 默认 10 位 */
}

hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    (void)pin; (void)freq_hz;
    /* TODO: 实现 analogWrite 映射 */
    return HAL_OK;
}

hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty) {
    (void)pin; (void)duty;
    return HAL_OK;
}

/* ---- Interrupt ---- */

hal_err_t hal_gpio_attach_isr(uint32_t pin, hal_gpio_edge_t edge,
                              hal_isr_flag_t* flag) {
    (void)pin; (void)edge; (void)flag;
    /* TODO: 实现 attachInterrupt 映射（ISR 中只设 flag） */
    return HAL_OK;
}

hal_err_t hal_gpio_detach_isr(uint32_t pin) {
    (void)pin;
    return HAL_OK;
}

bool hal_isr_flag_take(hal_isr_flag_t* flag) {
    if (!flag || !*flag) return false;
    *flag = 0;
    return true;
}

/* ---- Capability ---- */

bool hal_cap_supported(hal_cap_id_t cap) {
    switch (cap) {
    case HAL_CAP_GPIO:
    case HAL_CAP_UART:
    case HAL_CAP_ADC:
    case HAL_CAP_PWM:
        return true;
    case HAL_CAP_FLASH:
    case HAL_CAP_GPIO_INTERRUPT:
    case HAL_CAP_TIME_MICROS:
    case HAL_CAP_SLEEP:
    case HAL_CAP_I2C:
    case HAL_CAP_SPI:
    default:
        return false;   /* 首期暂不支持 */
    }
}
