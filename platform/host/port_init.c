/**
 * @file platform/host/port_init.c
 * @brief 主机模拟器入口 — Phase 6.2 增强版
 *
 * 用法:
 *   camelino-host [options] file.camel
 *
 * 选项:
 *   --acc-only        仅输出 acc 值（差分测试用）
 *   --trace           逐条指令 trace（opcode + pc + acc）
 *   --gc-stats        退出时输出 GC 统计
 *   --heap-stats      退出时输出堆用量统计
 *   --version         输出版本信息
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
#include "value.h"
#include "opcodes.h"

/* ---- 外部声明 ---- */
extern void caml_init_primitives(void);
extern void caml_init_globals(void);
extern void caml_process_events(void);

/* ---- Opcode 名称表（调试用） ---- */
static const char* opcode_names[] = {
    [ACC0]= "ACC0", [ACC1]="ACC1", [ACC2]="ACC2", [ACC3]="ACC3",
    [ACC4]="ACC4", [ACC5]="ACC5", [ACC6]="ACC6", [ACC7]="ACC7",
    [ACC]="ACC", [PUSH]="PUSH",
    [PUSHACC0]="PUSHACC0", [PUSHACC1]="PUSHACC1", [PUSHACC2]="PUSHACC2",
    [PUSHACC3]="PUSHACC3", [PUSHACC4]="PUSHACC4", [PUSHACC5]="PUSHACC5",
    [PUSHACC6]="PUSHACC6", [PUSHACC7]="PUSHACC7", [PUSHACC]="PUSHACC",
    [POP]="POP", [ASSIGN]="ASSIGN",
    [ENVACC1]="ENVACC1", [ENVACC2]="ENVACC2", [ENVACC3]="ENVACC3",
    [ENVACC4]="ENVACC4", [ENVACC]="ENVACC",
    [PUSHENVACC1]="PUSHENVACC1", [PUSHENVACC2]="PUSHENVACC2",
    [PUSHENVACC3]="PUSHENVACC3", [PUSHENVACC4]="PUSHENVACC4",
    [PUSHENVACC]="PUSHENVACC",
    [PUSH_RETADDR]="PUSH_RETADDR", [APPLY]="APPLY",
    [APPLY1]="APPLY1", [APPLY2]="APPLY2", [APPLY3]="APPLY3",
    [APPTERM]="APPTERM", [APPTERM1]="APPTERM1",
    [APPTERM2]="APPTERM2", [APPTERM3]="APPTERM3",
    [RETURN]="RETURN", [RESTART]="RESTART", [GRAB]="GRAB",
    [CLOSURE]="CLOSURE", [CLOSUREREC]="CLOSUREREC",
    [PUSHCONSTINT]="PUSHCONSTINT",
    [CONST0]="CONST0", [CONST1]="CONST1", [CONST2]="CONST2",
    [CONST3]="CONST3", [CONSTINT]="CONSTINT",
    [NEGINT]="NEGINT", [ADDINT]="ADDINT", [SUBINT]="SUBINT",
    [MULINT]="MULINT", [DIVINT]="DIVINT", [MODINT]="MODINT",
    [ANDINT]="ANDINT", [ORINT]="ORINT", [XORINT]="XORINT",
    [LSLINT]="LSLINT", [LSRINT]="LSRINT", [ASRINT]="ASRINT",
    [EQ]="EQ", [NEQ]="NEQ", [LTINT]="LTINT", [LEINT]="LEINT",
    [GTINT]="GTINT", [GEINT]="GEINT",
    [OFFSETINT]="OFFSETINT", [OFFSETREF]="OFFSETREF", [ISINT]="ISINT",
    [BEQ]="BEQ", [BNEQ]="BNEQ", [BLTINT]="BLTINT", [BLEINT]="BLEINT",
    [BGTINT]="BGTINT", [BGEINT]="BGEINT",
    [ULTINT]="ULTINT", [UGEINT]="UGEINT",
    [GETFIELD0]="GETFIELD0", [GETFIELD1]="GETFIELD1",
    [GETFIELD2]="GETFIELD2", [GETFIELD3]="GETFIELD3",
    [SETFIELD0]="SETFIELD0", [SETFIELD1]="SETFIELD1",
    [SETFIELD2]="SETFIELD2", [SETFIELD3]="SETFIELD3",
    [GETFLOATFIELD]="GETFLOATFIELD", [SETFLOATFIELD]="SETFLOATFIELD",
    [VECTLENGTH]="VECTLENGTH",
    [BRANCH]="BRANCH", [BRANCHIF]="BRANCHIF",
    [BRANCHIFNOT]="BRANCHIFNOT", [SWITCH]="SWITCH",
    [BOOLNOT]="BOOLNOT",
    [PUSHTRAP]="PUSHTRAP", [POPTRAP]="POPTRAP",
    [RAISE]="RAISE", [RAISE_NOTRACE]="RAISE_NOTRACE",
    [RERAISE]="RERAISE", [CHECK_SIGNALS]="CHECK_SIGNALS",
    [C_CALL1]="C_CALL1", [C_CALL2]="C_CALL2", [C_CALL3]="C_CALL3",
    [C_CALLN]="C_CALLN",
    [MAKEBLOCK1]="MAKEBLOCK1", [MAKEBLOCK2]="MAKEBLOCK2",
    [MAKEBLOCK3]="MAKEBLOCK3", [MAKEFLOATBLOCK]="MAKEFLOATBLOCK",
    [GETGLOBAL]="GETGLOBAL", [SETGLOBAL]="SETGLOBAL",
    [GETGLOBALFIELD]="GETGLOBALFIELD",
    [PUSHGETGLOBAL]="PUSHGETGLOBAL",
    [PUSHGETGLOBALFIELD]="PUSHGETGLOBALFIELD",
    [ATOM]="ATOM", [ATOM0]="ATOM0",
    [PUSHATOM]="PUSHATOM", [PUSHATOM0]="PUSHATOM0",
    [STOP]="STOP", [EVENT]="EVENT", [BREAK]="BREAK",
};

static const char* opcode_name(uint8_t op) {
    if (op < sizeof(opcode_names)/sizeof(opcode_names[0]) && opcode_names[op])
        return opcode_names[op];
    return "???";
}

/* ---- Trace hook ---- */
static int trace_enabled = 0;
static void trace_callback(uint8_t op, size_t pc_offset, value acc) {
    if (!trace_enabled) return;
    fprintf(stderr, "[TRACE] pc=0x%04zx op=%-14s (%3d) acc=%ld\n",
            pc_offset, opcode_name(op), op, (long)Long_val(acc));
}

/* ---- Load & Run ---- */
static int load_and_run(const char* filename, int acc_only,
                        int show_gc_stats, int show_heap_stats) {
    FILE* f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "ERROR: cannot open %s\n", filename); return 2; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc((size_t)len);
    if (!buf) { fclose(f); return 3; }
    size_t n = fread(buf, 1, (size_t)len, f); fclose(f);
    if (n != (size_t)len) { free(buf); return 4; }

    int rc = caml_bytecode_load(buf, (size_t)len);
    if (rc != CAML_BC_OK) {
        fprintf(stderr, "ERROR: bytecode load failed, code=%d\n", rc);
        free(buf); return rc;
    }

    caml_vm_init();
    if (trace_enabled) caml_set_trace_hook(trace_callback);
    caml_init_primitives();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(), NULL, 0);
    caml_init_globals();
    caml_interpret();

    value result = caml_get_acc();
    if (acc_only) {
        printf("%ld\n", (long)Long_val(result));
    } else {
        printf("acc = %ld\n", (long)Long_val(result));
    }

    /* 性能统计 */
    if (!acc_only) {
        printf("instructions = %zu\n", caml_get_instruction_count());
    }

    if (show_gc_stats) {
        caml_gc_stats_t gs;
        caml_get_gc_stats(&gs);
        printf("gc_collections = %d\n", gs.collections);
        printf("gc_freed_blocks = %zu\n", gs.freed_blocks);
        printf("gc_bytes_freed = %zu\n", gs.bytes_freed);
        printf("gc_bytes_live = %zu\n", gs.bytes_live_after);
    }

    if (show_heap_stats) {
        printf("heap_used_bytes = %zu\n", caml_heap_used());
        printf("heap_total_bytes = %zu\n", caml_heap_total());
        size_t pct = caml_heap_used() * 100 / caml_heap_total();
        printf("heap_usage_pct = %zu%%\n", pct);
    }

    caml_bytecode_unload();
    free(buf);
    return 0;
}

/* ---- Main ---- */
int main(int argc, char* argv[]) {
    int acc_only = 0, show_gc = 0, show_heap = 0, show_version = 0;
    const char* filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--acc-only") == 0)        acc_only = 1;
        else if (strcmp(argv[i], "--trace") == 0)      trace_enabled = 1;
        else if (strcmp(argv[i], "--gc-stats") == 0)   show_gc = 1;
        else if (strcmp(argv[i], "--heap-stats") == 0) show_heap = 1;
        else if (strcmp(argv[i], "--version") == 0)    show_version = 1;
        else if (argv[i][0] != '-')                    filename = argv[i];
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    if (show_version) {
        printf("camelino-host v0.2.0 (Phase 6.2)\n");
        printf("  ZAM interpreter — 125+ opcodes\n");
        printf("  Host simulator for differential testing\n");
        if (!filename) return 0;
    }

    if (!filename) {
        fprintf(stderr,
            "Usage: camelino-host [options] file.camel\n"
            "Options:\n"
            "  --acc-only     Output only acc value (for diff tests)\n"
            "  --trace        Trace every opcode to stderr\n"
            "  --gc-stats     Print GC statistics on exit\n"
            "  --heap-stats   Print heap usage on exit\n"
            "  --version      Print version info\n");
        return 1;
    }

    return load_and_run(filename, acc_only, show_gc, show_heap);
}
