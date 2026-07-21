/**
 * @file bindings/error.c
 * @brief HAL 错误码 → OCaml 异常映射
 *
 * 每个 binding 调用 HAL 后检查返回值，HAL_OK 以外的错误按语义映射。
 * 对应 Work.md 4.4.5，Design.md §7.6.2 的"错误统一返回码"原则。
 */

#include "hal/err.h"

/* TODO: Phase 4 实现 —— 当前为桩 */
/* 映射规则（见 Design.md §7.6.2）：
   HAL_ERR_INVAL       → OCaml Invalid_argument
   HAL_ERR_TIMEOUT     → OCaml Sys_error("timeout")
   HAL_ERR_UNSUPPORTED → OCaml Failure("unsupported")
   HAL_ERR_IO          → OCaml Sys_error(...)
   HAL_ERR_BUSY        → OCaml Failure("busy")
*/
