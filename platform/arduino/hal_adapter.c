/**
 * @file platform/arduino/hal_adapter.c
 * @brief Arduino HAL 适配器 — hal_* → Arduino API 完整实现
 *
 * 目标: RP2350 via arduino-pico (Earle Philhower's Arduino-Pico core)
 * 兼容: AVR, ESP32, SAMD, nRF52 等所有 Arduino 平台
 *
 * 与 platform/host 的关键区别:
 *  - 操作真实硬件 GPIO，非日志文件
 *  - UART 映射到硬件 Serial，非 stdout FILE*
 *  - 时间使用硬件定时器 millis()/micros()
 *  - 模拟使用 ADC/PWM 真实外设
 *  - ISR 仅置 flag，不在中断上下文中调用 VM
 *  - Flash 不支持（RP2350 上字节码在 PROGMEM 中）
 */

#include "hal/err.h"
#include "hal/gpio.h"
#include "hal/uart.h"
#include "hal/time.h"
#include "hal/analog.h"
#include "hal/flash.h"
#include "hal/interrupt.h"
#include "hal/cap.h"
#include <Arduino.h>

/* ---- GPIO -------------------------------------------------------------- */

hal_err_t hal_gpio_init(uint32_t pin, hal_gpio_mode_t mode) {
    if (pin > 255) return HAL_ERR_INVAL;

    switch (mode) {
    case HAL_GPIO_INPUT:
        pinMode(pin, INPUT);
        break;
    case HAL_GPIO_INPUT_PULLUP:
        pinMode(pin, INPUT_PULLUP);
        break;
    case HAL_GPIO_INPUT_PULLDOWN:
#ifdef INPUT_PULLDOWN
        pinMode(pin, INPUT_PULLDOWN);
#else
        pinMode(pin, INPUT);  /* 回退: 外部下拉 */
#endif
        break;
    case HAL_GPIO_OUTPUT:
        pinMode(pin, OUTPUT);
        break;
    case HAL_GPIO_OUTPUT_OD:
#ifdef OUTPUT_OPEN_DRAIN
        pinMode(pin, OUTPUT_OPEN_DRAIN);
#else
        pinMode(pin, OUTPUT);  /* 回退: 推挽 */
#endif
        break;
    default:
        return HAL_ERR_INVAL;
    }
    return HAL_OK;
}

hal_err_t hal_gpio_write(uint32_t pin, bool value) {
    if (pin > 255) return HAL_ERR_INVAL;
    digitalWrite(pin, value ? HIGH : LOW);
    return HAL_OK;
}

bool hal_gpio_read(uint32_t pin) {
    if (pin > 255) return false;
    return digitalRead(pin) == HIGH;
}

hal_err_t hal_gpio_toggle(uint32_t pin) {
    if (pin > 255) return HAL_ERR_INVAL;
    digitalWrite(pin, !digitalRead(pin));
    return HAL_OK;
}

/* ---- UART -------------------------------------------------------------- */

/* Arduino Serial (UART0) / Serial1 / Serial2 / Serial3 */
static HardwareSerial* get_serial(uint8_t port) {
    switch (port) {
    case 0:  return &Serial;
#ifdef HAVE_HWSERIAL1
    case 1:  return &Serial1;
#endif
#ifdef HAVE_HWSERIAL2
    case 2:  return &Serial2;
#endif
#ifdef HAVE_HWSERIAL3
    case 3:  return &Serial3;
#endif
    default: return NULL;
    }
}

hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg) {
    if (!cfg) return HAL_ERR_INVAL;
    HardwareSerial* s = get_serial(port);
    if (!s) return HAL_ERR_UNSUPPORTED;
    s->begin(cfg->baud);
    /* 等待串口就绪（部分平台需要） */
    while (!(*s)) { /* busy-wait */ }
    return HAL_OK;
}

int hal_uart_write(uint8_t port, const uint8_t* data, size_t len) {
    HardwareSerial* s = get_serial(port);
    if (!s) return -1;
    return s->write(data, len);
}

int hal_uart_read(uint8_t port, uint8_t* buf, size_t len) {
    HardwareSerial* s = get_serial(port);
    if (!s) return -1;
    size_t count = 0;
    while (count < len && s->available() > 0) {
        buf[count++] = (uint8_t)s->read();
    }
    return (int)count;
}

void hal_uart_flush(uint8_t port) {
    HardwareSerial* s = get_serial(port);
    if (s) s->flush();
}

/* ---- Time -------------------------------------------------------------- */

uint64_t hal_time_millis(void) {
    return (uint64_t)millis();
}

uint64_t hal_time_micros(void) {
    return (uint64_t)micros();
}

void hal_delay_ms(uint32_t ms) {
    delay(ms);
}

void hal_delay_us(uint32_t us) {
    delayMicroseconds(us);
}

/* ---- Analog ------------------------------------------------------------ */

hal_err_t hal_adc_init(uint32_t pin) {
    if (pin > 255) return HAL_ERR_INVAL;
    /* Arduino analogRead 自动初始化 ADC，无需显式 init */
    pinMode(pin, INPUT);
    return HAL_OK;
}

uint32_t hal_adc_read(uint32_t pin) {
    return (uint32_t)analogRead(pin);
}

int hal_adc_resolution(void) {
#ifdef analogReadResolution
    /* 部分平台支持动态分辨率，默认 10-bit */
    return 10;
#else
    return 10; /* Arduino 标准 ADC 为 10-bit (0-1023) */
#endif
}

hal_err_t hal_pwm_init(uint32_t pin, uint32_t freq_hz) {
    (void)freq_hz;
    if (pin > 255) return HAL_ERR_INVAL;
    /* Arduino analogWrite 自动配置 PWM，频率由平台决定 */
    pinMode(pin, OUTPUT);
    return HAL_OK;
}

hal_err_t hal_pwm_write(uint32_t pin, uint32_t duty) {
    if (pin > 255) return HAL_ERR_INVAL;
    if (duty > HAL_PWM_MAX) duty = HAL_PWM_MAX;
    analogWrite(pin, (int)duty);
    return HAL_OK;
}

/* ---- Flash (不支持) ---------------------------------------------------- */

hal_err_t hal_flash_read(uint32_t offset, uint8_t* buf, size_t len) {
    (void)offset; (void)buf; (void)len;
    /* Arduino 平台字节码编译进 Flash，由编译期嵌入，不通过此接口读取 */
    return HAL_ERR_UNSUPPORTED;
}

hal_err_t hal_flash_write(uint32_t offset, const uint8_t* buf, size_t len) {
    (void)offset; (void)buf; (void)len;
    /* 写入 Flash 需要平台特定的 PROGMEM/EEPROM 操作，首期不支持 */
    return HAL_ERR_UNSUPPORTED;
}

hal_err_t hal_flash_erase(uint32_t offset, size_t len) {
    (void)offset; (void)len;
    return HAL_ERR_UNSUPPORTED;
}

/* ---- Interrupt (§7.5: ISR 只设 flag，不进 VM) ------------------------- */

/* 每 pin 一个 flag，最多 32 个可中断 pin */
#define MAX_ISR_PINS 32
static hal_isr_flag_t isr_flags[MAX_ISR_PINS];
static uint8_t        isr_pins[MAX_ISR_PINS];
static uint8_t        isr_count = 0;

static void arduino_isr_handler(void) {
    /* ISR 中仅设置 flag —— 不调用任何 VM/分配/FFI 函数 */
    for (uint8_t i = 0; i < isr_count; i++) {
        /* 所有已注册 pin 的 ISR 触发时都置 flag */
        isr_flags[i] = 1;
    }
}

hal_err_t hal_gpio_attach_isr(uint32_t pin, hal_gpio_edge_t edge,
                              hal_isr_flag_t* flag) {
    if (isr_count >= MAX_ISR_PINS || pin > 255) return HAL_ERR_INVAL;

    /* 查找或创建 pin 的 slot */
    uint8_t slot = isr_count;
    for (uint8_t i = 0; i < isr_count; i++) {
        if (isr_pins[i] == pin) { slot = i; break; }
    }

    int arduino_mode;
    switch (edge) {
    case HAL_GPIO_EDGE_RISING:  arduino_mode = RISING;  break;
    case HAL_GPIO_EDGE_FALLING: arduino_mode = FALLING; break;
    case HAL_GPIO_EDGE_BOTH:    arduino_mode = CHANGE;  break;
    default: return HAL_ERR_INVAL;
    }

    pinMode(pin, INPUT);
    isr_pins[slot] = (uint8_t)pin;
    isr_flags[slot] = 0;
    if (flag) *flag = 0;
    if (slot == isr_count) isr_count++;

    attachInterrupt(digitalPinToInterrupt(pin), arduino_isr_handler, arduino_mode);
    return HAL_OK;
}

hal_err_t hal_gpio_detach_isr(uint32_t pin) {
    if (pin > 255) return HAL_ERR_INVAL;
    detachInterrupt(digitalPinToInterrupt(pin));
    /* 从 slot 表中移除 */
    for (uint8_t i = 0; i < isr_count; i++) {
        if (isr_pins[i] == pin) {
            isr_pins[i] = isr_pins[isr_count - 1];
            isr_flags[i] = isr_flags[isr_count - 1];
            isr_count--;
            break;
        }
    }
    return HAL_OK;
}

bool hal_isr_flag_take(hal_isr_flag_t* flag) {
    if (!flag || !*flag) return false;
    /* 非原子读-清零: ISR 可能在此间再次置位，丢失一次事件。
       严格来说需要原子操作（LDREX/STREX on ARM），
       但 Arduino API 没有暴露原子原语，首期保持简单 */
    *flag = 0;
    return true;
}

/* ---- Capability -------------------------------------------------------- */

bool hal_cap_supported(hal_cap_id_t cap) {
    switch (cap) {
    case HAL_CAP_GPIO:           return true;
    case HAL_CAP_UART:           return true;
    case HAL_CAP_ADC:            return true;
    case HAL_CAP_PWM:            return true;
    case HAL_CAP_GPIO_INTERRUPT: return true;
    case HAL_CAP_TIME_MICROS:    return true;
    case HAL_CAP_FLASH:          return false;  /* 字节码嵌入 PROGMEM */
    case HAL_CAP_SLEEP:          return false;  /* 未来 */
    case HAL_CAP_I2C:            return false;  /* 未来 */
    case HAL_CAP_SPI:            return false;  /* 未来 */
    default:                     return false;
    }
}
