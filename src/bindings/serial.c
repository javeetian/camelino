/**
 * @file bindings/serial.c
 * @brief UART CAMLprim 包装层 — OCaml value <-> C 转换 + 调 HAL
 *
 * 只调用 hal_uart_*，不直接碰平台 SDK。
 * Phase 4 实现完整的 CAMLprim + GC 根保护。
 */
#include "hal/uart.h"
#include "hal/err.h"

/* ---- Phase 4 实现 CAMLprim 入口 ---- */
/* TODO: caml_camellino_serial_begin, caml_camellino_serial_write, caml_camellino_serial_read */

