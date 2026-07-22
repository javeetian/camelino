/**
 * @file src/repl/history.c
 * @brief 命令历史 — 环形缓冲区，上下键浏览
 *
 * 固定大小环形缓冲，去重，支持 prev/next 导航。
 */

#include <string.h>

#define HIST_MAX     20
#define HIST_LINE_SZ 256

static char history[HIST_MAX][HIST_LINE_SZ];
static int  count = 0;
static int  cursor = -1;
static int  write_pos = 0;

void hist_init(void) {
    memset(history, 0, sizeof(history));
    count = 0; cursor = -1; write_pos = 0;
}

void hist_add(const char* line) {
    if (!line || !line[0]) return;
    int last = (write_pos > 0) ? write_pos - 1 : HIST_MAX - 1;
    if (count > 0 && strncmp(history[last], line, HIST_LINE_SZ - 1) == 0)
        return;
    strncpy(history[write_pos], line, HIST_LINE_SZ - 1);
    history[write_pos][HIST_LINE_SZ - 1] = '\0';
    write_pos = (write_pos + 1) % HIST_MAX;
    if (count < HIST_MAX) count++;
    cursor = -1;
}

const char* hist_prev(void) {
    if (count == 0) return NULL;
    if (cursor < 0) cursor = (write_pos > 0) ? write_pos - 1 : HIST_MAX - 1;
    else if (cursor > 0) cursor--;
    else cursor = HIST_MAX - 1;
    int dist = (write_pos - cursor + HIST_MAX) % HIST_MAX;
    if (dist > count) { cursor = -1; return NULL; }
    return history[cursor];
}

const char* hist_next(void) {
    if (count == 0 || cursor < 0) return NULL;
    cursor = (cursor + 1) % HIST_MAX;
    if (cursor == write_pos) { cursor = -1; return NULL; }
    return history[cursor];
}

void hist_reset_cursor(void) { cursor = -1; }
int  hist_count(void)         { return count; }
