/**
 * @file core/byteorder.h
 * @brief 字节序显式读写宏 (§1.4 可移植性原则)
 *
 * 所有多字节整数、double 的访问必须经过这些宏。
 * 编译期由 CAMELINO_BIG_ENDIAN 选择字节序。
 * 禁止裸指针解引用多字节类型。
 */

#ifndef CAMELINO_CORE_BYTEORDER_H
#define CAMELINO_CORE_BYTEORDER_H

#include <stdint.h>
#include <string.h>

/* ---- 编译期字节序选择 ---- */

#ifndef CAMELINO_BIG_ENDIAN
#  error "CAMELINO_BIG_ENDIAN must be defined (0=little, 1=big) by platform_config.h"
#endif

/* ---- 读取多字节整数（从字节流，如 bytecode pc[]） ---- */

static inline uint16_t caml_read_uint16(const uint8_t* p) {
#if CAMELINO_BIG_ENDIAN
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
#else
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
#endif
}

static inline uint32_t caml_read_uint32(const uint8_t* p) {
#if CAMELINO_BIG_ENDIAN
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
#else
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
#endif
}

static inline uint64_t caml_read_uint64(const uint8_t* p) {
#if CAMELINO_BIG_ENDIAN
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
         | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
         | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
         | ((uint64_t)p[6] << 8)  |  (uint64_t)p[7];
#else
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
#endif
}

static inline int16_t caml_read_int16(const uint8_t* p) {
    return (int16_t)caml_read_uint16(p);
}

static inline int32_t caml_read_int32(const uint8_t* p) {
    return (int32_t)caml_read_uint32(p);
}

static inline int64_t caml_read_int64(const uint8_t* p) {
    return (int64_t)caml_read_uint64(p);
}

/* ---- double 读取：memcpy，不允许假设对齐 (§3.4.3) ---- */

static inline double caml_read_double(const uint8_t* p) {
    double d;
    memcpy(&d, p, sizeof(double));
    return d;
}

/* ---- 别名（兼容 OCaml 风格命名） ---- */

#define CAML_READ_UINT16(p)  caml_read_uint16(p)
#define CAML_READ_UINT32(p)  caml_read_uint32(p)
#define CAML_READ_UINT64(p)  caml_read_uint64(p)
#define CAML_READ_INT16(p)   caml_read_int16(p)
#define CAML_READ_INT32(p)   caml_read_int32(p)
#define CAML_READ_INT64(p)   caml_read_int64(p)
#define CAML_READ_DOUBLE(p)  caml_read_double(p)

#endif /* CAMELINO_CORE_BYTEORDER_H */
