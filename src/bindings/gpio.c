/**
 * @file bindings/gpio.c
 * @brief GPIO CAMLprim 包装层 — OCaml value <-> C 转换 + 调 HAL
 *
 * 只调用 hal_*，不直接碰 Arduino API 或其他平台 SDK。
 * Phase 4 实现完整的 CAMLprim + GC 根保护。
 */
#include "hal/gpio.h"
#include "hal/err.h"

/* ---- Phase 4 实现 CAMLprim 入口 ---- */
/* TODO: caml_camellino_digital_write, caml_camellino_digital_read, caml_camellino_pin_mode */

