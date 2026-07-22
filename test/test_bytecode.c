/**
 * @file test/test_bytecode.c
 * @brief .camel 字节码加载器测试（Phase 1.5）
 *
 * 测试：格式解析、magic/word_size/CRC 校验、VM 集成
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/bytecode.h"
#include "../src/core/opcodes.h"

#define TEST(name)     void name(void)
#define RUN_TEST(name) do { printf("  %-35s", #name); name(); printf("OK\n"); } while(0)
#define ASSERT(expr)   assert(expr)

/* ---- 辅助：CRC-32 计算（与 bytecode.c 同算法，用于测试） ---- */

static uint32_t crc32_table[256];
static int crc_built = 0;
static void crc_init(void) {
    if (crc_built) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc_built = 1;
}
static uint32_t crc32(const uint8_t* d, size_t n) {
    crc_init();
    uint32_t c = 0xFFFFFFFFUL;
    for (size_t i = 0; i < n; i++) c = crc32_table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFUL;
}

/* ---- 辅助：构建 .camel 缓冲区 ---- */

typedef struct { uint8_t* buf; size_t len; } camel_buf_t;

/* 填充 uint32 LE */
static void w32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* forward decl of make_camel using struct (defined below after helpers) */

/* 构建最小 .camel 文件：header(32) + code + crc32 */
static camel_buf_t make_camel(const uint8_t* code, size_t code_sz) {
    size_t total = sizeof(camelino_header_t) + code_sz + 4;
    uint8_t* buf = (uint8_t*)calloc(1, total);
    assert(buf);

    camelino_header_t* hdr = (camelino_header_t*)buf;
    memcpy(hdr->magic, CAMELINO_MAGIC, CAMELINO_MAGIC_LEN);
    hdr->format_ver  = CAMELINO_FORMAT_VER;
    hdr->word_size   = (uint8_t)sizeof(value);
    hdr->big_endian  = CAMELINO_BIG_ENDIAN;
    hdr->code_size   = (uint32_t)code_sz;
    hdr->data_size   = 0;
    hdr->entry_offset = 0;

    /* code */
    memcpy(buf + sizeof(camelino_header_t), code, code_sz);

    /* crc32 of code[] */
    uint32_t c = crc32(buf + sizeof(camelino_header_t), code_sz);
    uint8_t* crc_pos = buf + sizeof(camelino_header_t) + code_sz;
    crc_pos[0] = (uint8_t)(c);
    crc_pos[1] = (uint8_t)(c >> 8);
    crc_pos[2] = (uint8_t)(c >> 16);
    crc_pos[3] = (uint8_t)(c >> 24);

    camel_buf_t r = { buf, total };
    return r;
}

/* ---- 测试 1：正常加载 2+3 字节码 ---- */

TEST(test_load_simple) {
    /* bytecode: CONST3, PUSH, CONST2, ADDINT, STOP → acc = 5 */
    uint8_t code[] = { CONST3, PUSH, CONST2, ADDINT, STOP };
    camel_buf_t cb = make_camel(code, sizeof(code));

    int rc = caml_bytecode_load(cb.buf, cb.len);
    ASSERT(rc == CAML_BC_OK);

    /* 跑 VM */
    caml_vm_init();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(), NULL, 0);
    caml_interpret();
    ASSERT(Long_val(caml_get_acc()) == 5);

    caml_bytecode_unload();
    free(cb.buf);
}

/* ---- 测试 2：bad magic ---- */

TEST(test_bad_magic) {
    uint8_t code[] = { CONST0, STOP };
    camel_buf_t cb = make_camel(code, sizeof(code));
    cb.buf[0] = 'X';  /* corrupt magic */

    int rc = caml_bytecode_load(cb.buf, cb.len);
    ASSERT(rc == CAML_BC_ERR_BAD_MAGIC);

    free(cb.buf);
}

/* ---- 测试 3：wrong word size ---- */

TEST(test_wrong_word_size) {
    uint8_t code[] = { CONST0, STOP };
    camel_buf_t cb = make_camel(code, sizeof(code));
    cb.buf[12] = 99;  /* impossible word size */

    int rc = caml_bytecode_load(cb.buf, cb.len);
    ASSERT(rc == CAML_BC_ERR_WORD_SIZE);

    free(cb.buf);
}

/* ---- 测试 4：bad CRC ---- */

TEST(test_bad_crc) {
    uint8_t code[] = { CONST0, STOP };
    camel_buf_t cb = make_camel(code, sizeof(code));
    /* flip a byte in code section */
    cb.buf[CAMELINO_HDR_SIZE + 1] ^= 0xFF;

    int rc = caml_bytecode_load(cb.buf, cb.len);
    ASSERT(rc == CAML_BC_ERR_CHECKSUM);

    free(cb.buf);
}

/* ---- 测试 5：too small buffer ---- */

TEST(test_too_small) {
    uint8_t tiny[4] = { 0 };
    int rc = caml_bytecode_load(tiny, 4);
    ASSERT(rc == CAML_BC_ERR_TOO_SMALL);
}

/* ---- 测试 6：entry_offset 指向 code 内（默认为 0）---- */

TEST(test_entry_offset_default) {
    uint8_t code[] = { CONST2, STOP };
    camel_buf_t cb = make_camel(code, sizeof(code));

    int rc = caml_bytecode_load(cb.buf, cb.len);
    ASSERT(rc == CAML_BC_OK);
    ASSERT(caml_bytecode_entry() == 0);

    caml_bytecode_unload();
    free(cb.buf);
}

/* ---- 测试 7：重复加载覆盖 ---- */

TEST(test_reload) {
    uint8_t code_a[] = { CONST1, STOP };
    camel_buf_t ca = make_camel(code_a, sizeof(code_a));
    ASSERT(caml_bytecode_load(ca.buf, ca.len) == CAML_BC_OK);
    ASSERT(caml_bytecode_code_size() == 2);

    uint8_t code_b[] = { CONST2, CONST3, ADDINT, STOP };
    camel_buf_t cb = make_camel(code_b, sizeof(code_b));
    ASSERT(caml_bytecode_load(cb.buf, cb.len) == CAML_BC_OK);
    ASSERT(caml_bytecode_code_size() == 4);

    caml_bytecode_unload();
    free(ca.buf); free(cb.buf);
}

int main(void) {
    printf("=== Camelino Bytecode Loader Tests ===\n\n");

    RUN_TEST(test_load_simple);
    RUN_TEST(test_bad_magic);
    RUN_TEST(test_wrong_word_size);
    RUN_TEST(test_bad_crc);
    RUN_TEST(test_too_small);
    RUN_TEST(test_entry_offset_default);
    RUN_TEST(test_reload);

    printf("\n=== All %d tests passed! ===\n", 7);
    return 0;
}
