/**
 * @file platform/host/hal_adapter.c
 * @brief 主机模拟器 HAL 适配器 — Phase 6.2 增强版
 *
 * hal_* → libc/POSIX 映射，附加:
 *  - GPIO 状态文件 (gpio_state.json) 用于差分测试断言
 *  - Flash 文件系统模拟 (flash.bin)
 */

#include "hal/err.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "hal/time.h"
#include "hal/analog.h"
#include "hal/flash.h"
#include "hal/interrupt.h"
#include "hal/cap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/time.h>
#endif

/* ---- GPIO State File ---- */
#define GPIO_STATE_FILE "gpio_state.json"

typedef struct {
    uint32_t pin;
    int      mode;     /* 0=IN, 1=OUT */
    int      value;    /* current state */
} gpio_pin_state_t;

static gpio_pin_state_t gpio_pins[32];
static int gpio_pin_count = 0;

static gpio_pin_state_t* find_pin(uint32_t pin) {
    for (int i = 0; i < gpio_pin_count; i++)
        if (gpio_pins[i].pin == pin) return &gpio_pins[i];
    if (gpio_pin_count < 32) {
        gpio_pins[gpio_pin_count].pin = pin;
        gpio_pins[gpio_pin_count].mode = 0;
        gpio_pins[gpio_pin_count].value = 0;
        return &gpio_pins[gpio_pin_count++];
    }
    return NULL;
}

static void save_gpio_state(void) {
    FILE* f = fopen(GPIO_STATE_FILE, "w");
    if (!f) return;
    fprintf(f, "{\n  \"pins\": [\n");
    for (int i = 0; i < gpio_pin_count; i++) {
        fprintf(f, "    {\"pin\":%u,\"mode\":%d,\"value\":%d}%s\n",
                gpio_pins[i].pin, gpio_pins[i].mode, gpio_pins[i].value,
                i < gpio_pin_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

/* ---- GPIO ---- */

hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode) {
    gpio_pin_state_t* p = find_pin(pin);
    if (!p) return HAL_ERR_UNSUPPORTED;
    p->mode = (mode == HAL_GPIO_OUTPUT || mode == HAL_GPIO_OUTPUT_OD) ? 1 : 0;
    p->value = 0;
    fprintf(stderr, "[HAL:gpio] pin=%u init mode=%d\n", pin, mode);
    save_gpio_state();
    return HAL_OK;
}

hal_err_t hal_gpio_write(uint32_t pin, bool value) {
    gpio_pin_state_t* p = find_pin(pin);
    if (!p) return HAL_ERR_UNSUPPORTED;
    p->value = (value ? 1 : 0);
    p->mode = 1;
    fprintf(stderr, "[HAL:gpio] pin=%u write=%d\n", pin, p->value);
    save_gpio_state();
    return HAL_OK;
}

bool hal_gpio_read(uint32_t pin) {
    gpio_pin_state_t* p = find_pin(pin);
    int v = (p ? p->value : 0);
    fprintf(stderr, "[HAL:gpio] pin=%u read=%d\n", pin, v);
    return (bool)v;
}

hal_err_t hal_gpio_toggle(uint32_t pin) {
    gpio_pin_state_t* p = find_pin(pin);
    if (!p) return HAL_ERR_UNSUPPORTED;
    p->value = !p->value;
    fprintf(stderr, "[HAL:gpio] pin=%u toggle -> %d\n", pin, p->value);
    save_gpio_state();
    return HAL_OK;
}

/* ---- UART ---- */

static FILE* uart_streams[4] = { NULL };

hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg) {
    (void)cfg;
    uart_streams[port] = (port == 0) ? stdout : stderr;
    return HAL_OK;
}

int hal_uart_write(uint8_t port, const uint8_t* data, size_t len) {
    FILE* f = (port < 4 && uart_streams[port]) ? uart_streams[port] : stdout;
    return (int)fwrite(data, 1, len, f);
}

int hal_uart_read(uint8_t port, uint8_t* buf, size_t len) {
    FILE* f = (port < 4 && uart_streams[port]) ? uart_streams[port] : stdin;
    if (!f || feof(f)) return 0;
    size_t r = fread(buf, 1, len, f);
    return (int)r;
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
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
    fprintf(stderr, "[HAL:adc] pin=%u init\n", pin);
    return HAL_OK;
}

uint32_t hal_adc_read(uint32_t pin) {
    fprintf(stderr, "[HAL:adc] pin=%u read\n", pin);
    return 0;
}

int hal_adc_resolution(void) { return 10; }

hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    fprintf(stderr, "[HAL:pwm] pin=%u init freq=%u\n", pin, freq_hz);
    return HAL_OK;
}

hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty) {
    fprintf(stderr, "[HAL:pwm] pin=%u duty=%u\n", pin, duty);
    return HAL_OK;
}

/* ---- Flash Filesystem Simulation ---- */
#define FLASH_SIM_FILE "flash.bin"
#define FLASH_SIM_SIZE  (512 * 1024)

static uint8_t* flash_data = NULL;

static void flash_ensure_loaded(void) {
    if (flash_data) return;
    flash_data = (uint8_t*)calloc(1, FLASH_SIM_SIZE);
    FILE* f = fopen(FLASH_SIM_FILE, "rb");
    if (f) {
        size_t r = fread(flash_data, 1, FLASH_SIM_SIZE, f);
        fclose(f);
        (void)r;
    }
}

static void flash_save(void) {
    if (!flash_data) return;
    FILE* f = fopen(FLASH_SIM_FILE, "wb");
    if (f) { fwrite(flash_data, 1, FLASH_SIM_SIZE, f); fclose(f); }
}

hal_err_t hal_flash_read(uint32_t offset, uint8_t* buf, size_t len) {
    flash_ensure_loaded();
    if (offset + len > FLASH_SIM_SIZE) return HAL_ERR_INVAL;
    memcpy(buf, flash_data + offset, len);
    return HAL_OK;
}

hal_err_t hal_flash_write(uint32_t offset, const uint8_t* buf, size_t len) {
    flash_ensure_loaded();
    if (offset + len > FLASH_SIM_SIZE) return HAL_ERR_INVAL;
    memcpy(flash_data + offset, buf, len);
    flash_save();
    return HAL_OK;
}

hal_err_t hal_flash_erase(uint32_t offset, size_t len) {
    flash_ensure_loaded();
    if (offset + len > FLASH_SIM_SIZE) return HAL_ERR_INVAL;
    memset(flash_data + offset, 0xFF, len);
    flash_save();
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
    case HAL_CAP_FLASH:
        return true;
    case HAL_CAP_ADC:
    case HAL_CAP_PWM:
    case HAL_CAP_GPIO_INTERRUPT:
    case HAL_CAP_SLEEP:
    case HAL_CAP_I2C:
    case HAL_CAP_SPI:
    default:
        return false;
    }
}
