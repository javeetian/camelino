/**
 * @file src/repl/repl.c
 * @brief 设备端 REPL — 串口接收 .camel 片段，加载并执行
 *
 * 设备端不做源码编译。PC 端 camelino-repl 负责 ocamlc→camelino-embed→.camel，
 * 通过串口发送到设备。设备端接收、校验、执行。
 *
 * 协议:
 *   1. PC 发送: "CAMELINO01" magic + header + code[] + CRC32 (标准 .camel 格式)
 *   2. 设备检测 magic，缓冲完整.camel，加载执行
 *   3. 设备回传: "OK:<value>" 或 "ERR:<code>"
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "platform.h"
#include "value.h"

#define CAMEL_MAGIC "CAMELINO01"
#define CAMEL_MAGIC_LEN 10
#define CAMEL_MAX_SIZE 65536

/* 输出回调: 由调用者设置（通常是 hal_uart_write 的单字节包装） */
static void (*repl_putc)(char) = NULL;
static int  (*repl_getc)(void)  = NULL;

/* .camel 接收缓冲区 */
static uint8_t camel_buf[CAMEL_MAX_SIZE];
static size_t  camel_len = 0;
static int     camel_ready = 0;

/* 状态 */
typedef enum { REPL_IDLE, REPL_RECEIVING } repl_state_t;
static repl_state_t state = REPL_IDLE;

/* ---- 外部依赖 ---- */
extern void caml_vm_init(void);
extern int  caml_bytecode_load(const uint8_t* buf, size_t len);
extern void caml_load_bytecode_buf(const uint8_t* code, size_t code_size,
                                    const uint8_t* data, size_t data_size);
extern void caml_init_primitives(void);
extern void caml_init_globals(void);
extern void caml_interpret(void);
extern value caml_get_acc(void);
extern int   caml_vm_halted(void);
extern void  caml_process_events(void);

extern const uint8_t* caml_bytecode_code(void);
extern size_t caml_bytecode_code_size(void);

/* ---- 行编辑器和历史的外部引用 ---- */
extern void le_init(void (*)(char));
extern int  le_handle_char(char);
extern const char* le_get_line(void);
extern int  le_is_empty(void);
extern void le_clear(void);
extern void hist_init(void);
extern void hist_add(const char*);
extern const char* hist_prev(void);
extern const char* hist_next(void);
extern void hist_reset_cursor(void);

/* ---- 初始化 ---- */
void caml_repl_init(void (*putc_fn)(char), int (*getc_fn)(void)) {
    repl_putc = putc_fn;
    repl_getc = getc_fn;
    state = REPL_IDLE;
    camel_len = 0;
    camel_ready = 0;

    /* 初始化行编辑器和历史 */
    if (repl_putc) le_init(repl_putc);
    hist_init();
}

/* ---- .camel 接收 ---- */

/* 重置接收状态 */
static void repl_rx_reset(void) {
    camel_len = 0;
    state = REPL_IDLE;
}

/* 检查 buffer 中是否包含完整的 .camel 文件
   返回需要接收的总字节数，0 表示尚不确定 */
static size_t repl_parse_header(const uint8_t* buf, size_t len) {
    if (len < 24) return 0;  /* 最小的 .camel header: magic(10)+format(2)+word(1)+endian(1)+reserved(2)+crc(4)+code(4)+data(4)+entry(4)=32? 实际: 10+2+1+1+2+4+4+4+4=32 */
    /* Verify magic at beginning */
    if (memcmp(buf, CAMEL_MAGIC, CAMEL_MAGIC_LEN) != 0) return 0;

    /* Read code_size from offset 20 (after: magic 10 + format_ver 2 + word 1 + endian 1 + reserved 2 + stdlib_crc 4 = 20) */
    size_t off = 20;
    uint32_t code_size = (uint32_t)buf[off] | ((uint32_t)buf[off+1] << 8)
                       | ((uint32_t)buf[off+2] << 16) | ((uint32_t)buf[off+3] << 24);
    off += 4;  /* now 24 */
    /* data_size at offset 24 */
    uint32_t data_size = (uint32_t)buf[off] | ((uint32_t)buf[off+1] << 8)
                       | ((uint32_t)buf[off+2] << 16) | ((uint32_t)buf[off+3] << 24);
    off += 4;  /* now 28 */
    /* entry_offset at 28 */
    off += 4;  /* now 32 */

    /* Total: header(32) + code_size + data_size + CRC32(4) */
    size_t total = 32 + (size_t)code_size + (size_t)data_size + 4;

    if (total >= CAMEL_MAX_SIZE) return 0;  /* too large */
    return total;
}

/* ---- 接收字节 ---- */
void caml_repl_feed_byte(uint8_t b) {
    if (camel_ready) return;  /* 上一份未取走 */

    if (camel_len < CAMEL_MAX_SIZE) {
        camel_buf[camel_len++] = b;
    }

    /* 检查 magic */
    if (camel_len >= CAMEL_MAGIC_LEN) {
        if (memcmp(camel_buf, CAMEL_MAGIC, CAMEL_MAGIC_LEN) == 0) {
            state = REPL_RECEIVING;
        } else if (state == REPL_IDLE) {
            /* 不是 .camel 数据 — 可能是普通字符（行编辑模式） */
            /* 将字节传给行编辑器 */
            le_handle_char((char)b);
            /* 每次 magic 检测失败时不移除数据，而是等待行编辑提交 */
            if (camel_len > CAMEL_MAGIC_LEN) {
                repl_rx_reset();
            }
            return;
        }
    }

    if (state == REPL_RECEIVING) {
        size_t need = repl_parse_header(camel_buf, camel_len);
        if (need > 0 && camel_len >= need) {
            camel_ready = 1;
        }
    }
}

/* ---- .camel 加载与执行 ---- */

int caml_repl_process(void) {
    /* 处理行编辑模式下的已提交行 */
    /* (本地命令行 REPL 使用，不做字节码编译) */

    /* 检查是否有完整 .camel 待执行 */
    if (!camel_ready) return 0;
    camel_ready = 0;

    /* 加载字节码 */
    int rc = caml_bytecode_load(camel_buf, camel_len);
    if (rc != 0) {
        if (repl_putc) {
            char err[32];
            snprintf(err, sizeof(err), "ERR:%d\r\n", rc);
            for (const char* p = err; *p; p++) repl_putc(*p);
        }
        repl_rx_reset();
        return rc;
    }

    /* 初始化 VM 并执行 */
    caml_vm_init();
    caml_init_primitives();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(),
                           NULL, 0);
    caml_init_globals();
    caml_interpret();

    /* 回传结果 */
    value result = caml_get_acc();
    if (repl_putc) {
        char out[64];
        snprintf(out, sizeof(out), "OK:%ld\r\n", (long)result);
        for (const char* p = out; *p; p++) repl_putc(*p);
    }

    repl_rx_reset();
    return 0;
}

/* ---- 消息回传 ---- */
void caml_repl_send_result(int64_t val) {
    if (!repl_putc) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "OK:%lld\r\n", (long long)val);
    for (const char* p = buf; *p; p++) repl_putc(*p);
}

void caml_repl_send_error(const char* msg) {
    if (!repl_putc) return;
    repl_putc('E'); repl_putc('R'); repl_putc('R'); repl_putc(':');
    for (const char* p = msg; *p; p++) repl_putc(*p);
    repl_putc('\r'); repl_putc('\n');
}
