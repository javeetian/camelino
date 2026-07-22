/**
 * @file platform/host/port_init.c
 * @brief 主机模拟器入口 — 加载 .camel，运行 VM，输出结果
 *
 * 用法：
 *   ./camelino-host [file.camel]          # 加载 .camel 文件执行
 *   ./camelino-host                        # 无参数打印 usage
 *   ./camelino-host --acc-only file.camel  # 仅输出 acc 值（差分测试用）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
#include "value.h"

/* ---- 外部声明（实现来自 core/） ---- */

extern void caml_init_primitives(void);
extern void caml_init_globals(void);
extern void caml_process_events(void);

/* ---- 入口 ---- */

static int load_and_run(const char* filename, int acc_only) {
    /* read file */
    FILE* f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "ERROR: cannot open %s\n", filename); return 2; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc((size_t)len);
    if (!buf) { fclose(f); return 3; }
    size_t n = fread(buf, 1, (size_t)len, f); fclose(f);
    if (n != (size_t)len) { free(buf); return 4; }

    /* load .camel */
    int rc = caml_bytecode_load(buf, (size_t)len);
    if (rc != CAML_BC_OK) {
        fprintf(stderr, "ERROR: bytecode load failed, code=%d\n", rc);
        free(buf); return rc;
    }

    /* init VM and run */
    caml_vm_init();
    caml_init_primitives();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(), NULL, 0);
    caml_init_globals();
    caml_interpret();

    /* output result */
    value result = caml_get_acc();
    if (acc_only) {
        printf("%ld\n", (long)Long_val(result));
    } else {
        printf("acc = %ld (tagged: %llu)\n", (long)Long_val(result),
               (unsigned long long)result);
    }

    caml_bytecode_unload();
    free(buf);
    return 0;
}

int main(int argc, char* argv[]) {
    int acc_only = 0;
    const char* filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--acc-only") == 0) {
            acc_only = 1;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "Usage: camelino-host [--acc-only] file.camel\n");
        return 1;
    }

    return load_and_run(filename, acc_only);
}
