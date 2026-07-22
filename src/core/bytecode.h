/**
 * @file core/bytecode.h
 * @brief .camel 字节码文件加载与校验（Design.md §4.4 格式 2）
 *
 * Phase 1：固定 32 字节头 + code[] + data[] + checksum 的简化格式。
 * 校验项：magic、word_size、checksum (CRC-32/ISO-HDLC)。
 *
 *
 * .camel 字节流格式 (v1, LE)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *   Offset   Size    Field
 *   ──────   ────    ─────────────────
 *     0       10     magic              "CAMELINO01"
 *    10        2     format_ver         1 (uint16 LE)
 *    12        1     word_size          sizeof(value): 4 or 8
 *    13        1     big_endian         0=LE, 1=BE
 *    14        2     reserved           {0,0}
 *    16        4     stdlib_crc         uint32 LE (Phase 1: ignored)
 *    20        4     code_size          uint32 LE
 *    24        4     data_size          uint32 LE
 *    28        4     entry_offset       uint32 LE, PC jump target in code[]
 *   ── 32 ────────────────────────────  header end
 *    32   code_sz   code[]             ZAM instruction stream
 *   ──────────────────────────────────
 *    +C   data_sz   data[]             global_data initial values
 *   ──────────────────────────────────
 *    end      4     checksum           CRC-32/ISO-HDLC of code[]+data[]
 *   ═══════════════════════════════════════════════════════════════════════════
 *
 *
 *   例子：2 + 3 字节码 (code_sz=5, data_sz=0, total=41)
 *   ──────────────────────────────────────────────────────
 *     0: 43 41 4D 45 4C 49 4E 4F 30 31   "CAMELINO01"
 *    10: 01 00                             format_ver=1
 *    12: 08                                word_size=8 (64-bit)
 *    13: 00                                big_endian=0
 *    14: 00 00                             reserved
 *    16: 00 00 00 00                       stdlib_crc=0
 *    20: 05 00 00 00                       code_size=5
 *    24: 00 00 00 00                       data_size=0
 *    28: 00 00 00 00                       entry_offset=0
 *   ── 32 ───────────────────────────────
 *    32: 66 09 65 6E 8F                    CONST3 PUSH CONST2 ADDINT STOP
 *   ── 37 ───────────────────────────────
 *    37: xx xx xx xx                       CRC32 of bytes[32..36]
 *   ═══════════════════════════════════════  total = 41 bytes
 */

#ifndef CAMELINO_BYTECODE_H
#define CAMELINO_BYTECODE_H

#include <stdint.h>
#include <stddef.h>

/* ---- .camel 头部常量 ---- */

#define CAMELINO_MAGIC        "CAMELINO01"
#define CAMELINO_MAGIC_LEN    10
#define CAMELINO_FORMAT_VER   1          /* 当前格式版本 */
#define CAMELINO_HDR_SIZE     32         /* 固定头大小 */

/* ---- .camel 头部结构（32 字节）---- */

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[10];       /* "CAMELINO01"                  */
    uint16_t format_ver;      /* 格式版本 (LE)                  */
    uint8_t  word_size;       /* sizeof(value): 4 或 8         */
    uint8_t  big_endian;      /* 0=LE, 1=BE                    */
    uint8_t  reserved[2];     /* 保留，须为 0                    */
    uint32_t stdlib_crc;      /* stdlib 子集 CRC32（Phase 1 忽略）*/
    uint32_t code_size;       /* 字节码指令流长度（字节，LE）      */
    uint32_t data_size;       /* 全局数据初值长度（字节，LE）    */
    uint32_t entry_offset;    /* 入口在 code[] 中的偏移（LE）     */
} camelino_header_t;
#pragma pack(pop)

/* ---- API ---- */

/**
 * 加载 .camel 字节码。
 * 返回 0 = 成功，非 0 = 校验失败码（见下方枚举）。
 * 成功后可通过 caml_bytecode_get_*() 获取数据指针。
 *
 * buf 必须保持有效直到 caml_bytecode_unload() 被调用
 * （loader 不复制数据，仅持有指针）。
 */
int  caml_bytecode_load(const uint8_t* buf, size_t len);
void caml_bytecode_unload(void);

/* 加载后的数据访问 */
const uint8_t* caml_bytecode_code(void);
size_t         caml_bytecode_code_size(void);
const uint8_t* caml_bytecode_data(void);
size_t         caml_bytecode_data_size(void);
uint32_t       caml_bytecode_entry(void);

/* 校验失败码 */
enum {
    CAML_BC_OK                = 0,
    CAML_BC_ERR_TOO_SMALL     = 1,   /* 数据长度不足头部 */
    CAML_BC_ERR_BAD_MAGIC     = 2,   /* magic 不匹配 */
    CAML_BC_ERR_WORD_SIZE     = 3,   /* word_size 与平台不一致 */
    CAML_BC_ERR_ENDIAN        = 4,   /* big_endian 不匹配 */
    CAML_BC_ERR_FORMAT_VER    = 5,   /* 格式版本不支持 */
    CAML_BC_ERR_CHECKSUM      = 6    /* CRC32 校验失败 */
};

#endif
