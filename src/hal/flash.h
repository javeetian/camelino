/**
 * @file hal/flash.h
 * @brief HAL Flash/PROM 抽象接口 — 字节码与持久化存储
 *
 * 部分平台（如裸机）可能不支持此 capability。
 */

#ifndef CAMELINO_HAL_FLASH_H
#define CAMELINO_HAL_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include "hal/err.h"

hal_err_t hal_flash_read (uint32_t offset, uint8_t* buf, size_t len);
hal_err_t hal_flash_write(uint32_t offset, const uint8_t* buf, size_t len);
hal_err_t hal_flash_erase(uint32_t offset, size_t len);

#endif
