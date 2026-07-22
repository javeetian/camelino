/**
 * @file core/bytecode.c
 * @brief .camel 字节码加载器实现
 *
 * 格式：32 字节头 + code[] + data[] + 4 字节 CRC32 checksum
 * CRC 覆盖范围：code[] + data[]
 */
#include "bytecode.h"
#include "camelino_config.h"
#include "platform.h"
#include "byteorder.h"
#include <string.h>

/* ---- 全局状态 ---- */

static const uint8_t*  bc_code    = NULL;
static size_t          bc_code_sz = 0;
static const uint8_t*  bc_data    = NULL;
static size_t          bc_data_sz = 0;
static uint32_t        bc_entry   = 0;

/* ---- CRC-32/ISO-HDLC (polynomial 0xEDB88320 reflected) ---- */

static uint32_t crc32_table[256];
static int      crc32_table_built = 0;

static void crc32_build_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (0xEDB88320UL ^ (crc >> 1)) : (crc >> 1);
        }
        crc32_table[i] = crc;
    }
    crc32_table_built = 1;
}

static uint32_t crc32_compute(const uint8_t* data, size_t len) {
    if (!crc32_table_built) crc32_build_table();
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ---- 解析与校验 ---- */

int caml_bytecode_load(const uint8_t* buf, size_t len) {
    if (len < CAMELINO_HDR_SIZE + 4) {
        return CAML_BC_ERR_TOO_SMALL;
    }

    const camelino_header_t* hdr = (const camelino_header_t*)buf;

    /* 1. magic */
    if (memcmp(hdr->magic, CAMELINO_MAGIC, CAMELINO_MAGIC_LEN) != 0) {
        return CAML_BC_ERR_BAD_MAGIC;
    }

    /* 2. format version */
    uint16_t ver = CAML_READ_UINT16((const uint8_t*)&hdr->format_ver);
    if (ver != CAMELINO_FORMAT_VER) {
        return CAML_BC_ERR_FORMAT_VER;
    }

    /* 3. word size */
    if (hdr->word_size != sizeof(value)) {
        return CAML_BC_ERR_WORD_SIZE;
    }

    /* 4. endianness */
    if (hdr->big_endian != CAMELINO_BIG_ENDIAN) {
        return CAML_BC_ERR_ENDIAN;
    }

    /* 5. 读取大小字段 */
    uint32_t code_sz = CAML_READ_UINT32((const uint8_t*)&hdr->code_size);
    uint32_t data_sz = CAML_READ_UINT32((const uint8_t*)&hdr->data_size);
    uint32_t entry   = CAML_READ_UINT32((const uint8_t*)&hdr->entry_offset);

    /* 6. 长度检查 */
    size_t expected = CAMELINO_HDR_SIZE + (size_t)code_sz + (size_t)data_sz + 4;
    if (len < expected) {
        return CAML_BC_ERR_TOO_SMALL;
    }

    /* 7. entry 范围检查 */
    if (entry >= code_sz && code_sz > 0) {
        return CAML_BC_ERR_TOO_SMALL;
    }

    /* 8. 定位数据段 */
    const uint8_t* code = buf + CAMELINO_HDR_SIZE;
    const uint8_t* data = code + code_sz;

    /* 9. CRC32 checksum 校验（cover code[] + data[]） */
    size_t body_len = (size_t)code_sz + (size_t)data_sz;
    uint32_t expected_crc = CAML_READ_UINT32(data + data_sz);
    uint32_t actual_crc   = crc32_compute(code, body_len);
    if (expected_crc != actual_crc) {
        return CAML_BC_ERR_CHECKSUM;
    }

    /* 通过全部校验：保存指针 */
    bc_code    = code;
    bc_code_sz = code_sz;
    bc_data    = (data_sz > 0) ? data : NULL;
    bc_data_sz = data_sz;
    bc_entry   = entry;

    return CAML_BC_OK;
}

void caml_bytecode_unload(void) {
    bc_code    = NULL;
    bc_code_sz = 0;
    bc_data    = NULL;
    bc_data_sz = 0;
    bc_entry   = 0;
}

/* ---- 访问器 ---- */

const uint8_t* caml_bytecode_code(void)      { return bc_code; }
size_t         caml_bytecode_code_size(void) { return bc_code_sz; }
const uint8_t* caml_bytecode_data(void)      { return bc_data; }
size_t         caml_bytecode_data_size(void) { return bc_data_sz; }
uint32_t       caml_bytecode_entry(void)     { return bc_entry; }
#include "../../src/core/platform.h"
#include "../../src/core/camelino_config.h"
#include "../../src/core/bytecode.h"
#include <stdio.h>
#include <stdlib.h>
int main(void){
 FILE* f = fopen("_t.camel", "rb"); fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
 uint8_t* buf = malloc(len); fread(buf,1,len,f); fclose(f);
 int rc = caml_bytecode_load(buf, len);
 fprintf(stderr, "load_rc=%d cs=%zu code[0]=%02x\n", rc, caml_bytecode_code_size(), caml_bytecode_code()?caml_bytecode_code()[0]:0);
 free(buf); return 0;
}
