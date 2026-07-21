/**
 * @file hal/err.h
 * @brief HAL 统一错误码 — 所有 HAL 接口共用的返回类型
 */

#ifndef CAMELINO_HAL_ERR_H
#define CAMELINO_HAL_ERR_H

typedef enum {
    HAL_OK          = 0,
    HAL_ERR_INVAL   = 1,   /* 无效参数（如 pin 不存在）  */
    HAL_ERR_TIMEOUT = 2,   /* 操作超时                  */
    HAL_ERR_UNSUPPORTED = 3, /* 该平台不支持此 capability */
    HAL_ERR_IO       = 4,  /* 底层 IO 错误               */
    HAL_ERR_BUSY     = 5   /* 资源正忙 / 不可用           */
} hal_err_t;

#endif
