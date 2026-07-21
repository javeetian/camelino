/**
 * @file hal/uart.h
 * @brief HAL UART/串口抽象接口
 */

#ifndef CAMELINO_HAL_UART_H
#define CAMELINO_HAL_UART_H

#include <stdint.h>
#include <stddef.h>
#include "hal/err.h"

typedef struct {
    uint32_t baud;
    uint32_t tx_pin;
    uint32_t rx_pin;
} hal_uart_config_t;

hal_err_t hal_uart_init(uint8_t port, const hal_uart_config_t* cfg);
int       hal_uart_write(uint8_t port, const uint8_t* data, size_t len);
int       hal_uart_read(uint8_t port, uint8_t* buf, size_t len);
void      hal_uart_flush(uint8_t port);

#endif
