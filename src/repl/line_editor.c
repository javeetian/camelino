/**
 * @file src/repl/line_editor.c
 * @brief 终端行编辑器 — 单行编辑，回显，退格，回车
 *
 * 用于设备端 REPL 的串口行编辑。固定缓冲区，无动态分配。
 */

#include <string.h>

#define LE_BUF_SIZE 256

typedef struct {
    char buf[LE_BUF_SIZE];
    int  pos;          /* 光标位置 (0..len) */
    int  len;          /* 当前行长度 */
    void (*putc)(char);/* 输出回调 (单字节写入 UART) */
} line_editor_t;

static line_editor_t le;

void le_init(void (*putc_fn)(char)) {
    memset(&le, 0, sizeof(le));
    le.putc = putc_fn;
}

void le_clear(void) {
    le.pos = 0; le.len = 0; le.buf[0] = '\0';
}

int le_handle_char(char c) {
    if (!le.putc) return 0;

    if (c == '\r' || c == '\n') {
        le.buf[le.len] = '\0'; le.putc('\r'); le.putc('\n'); return 1;
    }
    if (c == 0x7F || c == 0x08) {  /* Backspace */
        if (le.pos > 0) {
            le.pos--; le.len--;
            memmove(&le.buf[le.pos], &le.buf[le.pos + 1], le.len - le.pos);
            le.buf[le.len] = '\0';
            le.putc('\b'); le.putc(' '); le.putc('\b');
        }
        return 0;
    }
    if (c == 0x03) {  /* Ctrl+C */
        le_clear(); le.putc('^'); le.putc('C'); le.putc('\r'); le.putc('\n');
        return 0;
    }
    /* Printable chars */
    if ((unsigned char)c >= 0x20 && le.len < LE_BUF_SIZE - 1) {
        if (le.pos < le.len)
            memmove(&le.buf[le.pos + 1], &le.buf[le.pos], le.len - le.pos);
        le.buf[le.pos] = c; le.pos++; le.len++;
        le.buf[le.len] = '\0';
        le.putc(c);
    }
    return 0;
}

const char* le_get_line(void) { return le.buf; }
int le_is_empty(void)          { return le.len == 0; }
