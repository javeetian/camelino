/**
 * @file platform/host/hal_adapter.c
 * @brief 主机模拟器 HAL 适配器 — hal_* → libc/POSIX
 */

#include "hal/err.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "hal/time.h"
#include "hal/analog.h"
#include "hal/interrupt.h"
#include "hal/cap.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <time.h>
#endif

/* ---- GPIO ---- */

hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode) {
    (void)pin; (void)mode;
    return HAL_OK;
}

hal_err_t hal_gpio_write(uint32_t pin, bool value) {
    fprintf(stderr, "[HAL:gpio] pin=%u write=%d\n", pin, value);
    return HAL_OK;
}

bool hal_gpio_read(uint32_t pin) {
    fprintf(stderr, "[HAL:gpio] pin=%u read\n", pin);
    return false;
}

hal_err_t hal_gpio_toggle(uint32_t pin) {
    fprintf(stderr, "[HAL:gpio] pin=%u toggle\n", pin);
    return HAL_OK;
}

/* ---- UART ---- */

static FILE* uart_streams[4] = { NULL };

hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg) {
    (void)cfg;
    if (port == 0) {
        uart_streams[port] = stdout;
    } else {
        uart_streams[port] = stderr;
    }
    return HAL_OK;
}

int hal_uart_write(uint8_t port, const uint8_t* data, size_t len) {
    FILE* f = (port < 4 && uart_streams[port]) ? uart_streams[port] : stdout;
    return (int)fwrite(data, 1, len, f);
}

int hal_uart_read(uint8_t port, uint8_t* buf, size_t len) {
    (void)port; (void)buf; (void)len;
    return 0;
}

void hal_uart_flush(uint8_t port) {
    FILE* f = (port < 4 && uart_streams[port]) ? uart_streams[port] : stdout;
    fflush(f);
}

/* ---- Time ---- */

static uint64_t host_time_base_ms = 0;

static uint64_t host_time_now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

uint64_t hal_time_millis(void) {
    uint64_t now = host_time_now_ms();
    if (host_time_base_ms == 0) host_time_base_ms = now;
    return now - host_time_base_ms;
}

uint64_t hal_time_micros(void) {
#ifdef _WIN32
    return GetTickCount64() * 1000;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
#endif
}

void hal_delay_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void hal_delay_us(uint32_t us) {
#ifdef _WIN32
    if (us >= 1000) Sleep(us / 1000);
#else
    usleep(us);
#endif
}

/* ---- Analog ---- */

hal_err_t hal_adc_init(uint32_t pin) {
    (void)pin;
    return HAL_OK;
}

uint32_t hal_adc_read(uint32_t pin) {
    fprintf(stderr, "[HAL:adc] pin=%u read\n", pin);
    return 0;
}

int hal_adc_resolution(void) {
    return 10;
}

hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    (void)pin; (void)freq_hz;
    return HAL_OK;
}

hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty) {
    fprintf(stderr, "[HAL:pwm] pin=%u duty=%u\n", pin, duty);
    return HAL_OK;
}

/* ---- Interrupt ---- */

hal_err_t hal_gpio_attach_isr(uint32_t pin, hal_gpio_edge_t edge,
                              hal_isr_flag_t* flag) {
    (void)pin; (void)edge; (void)flag;
    return HAL_ERR_UNSUPPORTED;
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
    case HAL_CAP_TIME_MICROS:
        return true;
    case HAL_CAP_ADC:
    case HAL_CAP_PWM:
    case HAL_CAP_FLASH:
    case HAL_CAP_GPIO_INTERRUPT:
    case HAL_CAP_SLEEP:
    case HAL_CAP_I2C:
    case HAL_CAP_SPI:
    default:
        return false;
    }
}
