/**
 * @file core/value.c
 * @brief value.h 中非内联函数的实现
 */

#include "value.h"
#include <string.h>
#include <stdio.h>

/*==============================================================================
 * 调试输出
 *============================================================================*/

void caml_debug_print_value(value v, char* out, size_t out_size) {
    char* p = out;
    size_t remaining = out_size;
    int n;

    if (Is_long(v)) {
        n = snprintf(p, remaining, "%ld", (long)Long_val(v));
    } else {
        n = snprintf(p, remaining, "<block:%p>", (void*)v);
    }
    p += n;

    if ((size_t)n < out_size && remaining > (size_t)n) {
        *(out + (out_size - 1)) = '\0';
    }
}
/**
 * @file core/memory.c
 * @brief 静态内存池 + bump 分配器实现
 *
 * 堆块布局：
 *   [header_t header] [value fields...]
 *    ↑                  ↑
 *    alloc 基址         caml_alloc 返回值（field0 指针）
 *
 *   总字节 = (wosize + 1) * sizeof(value)
 */

#include "memory.h"
#include "value.h"
#include "error.h"
#include <stddef.h>

/* ---- 静态内存池（Design.md §6.2）---- */

static uint8_t  camelino_heap[HEAP_SIZE];
static value    camelino_stack[STACK_SIZE];
static value    camelino_globals[MAX_GLOBALS];

/* ---- 堆 bump 分配器 ---- */

static uint8_t* heap_ptr = NULL;    /* 当前 bump 指针（下一个 block 的起始） */
static uint8_t* heap_end = NULL;
static size_t   heap_used_bytes = 0;

void caml_memory_init(void) {
    heap_ptr = camelino_heap;
    heap_end = camelino_heap + HEAP_SIZE;
    heap_used_bytes = 0;

    caml_stack_init();
    caml_globals_init();
}

value caml_alloc(mlsize_t wosize, tag_t tag) {
    /* 总字节 = header + wosize 个 value */
    size_t total = (wosize + 1) * sizeof(value);

    /* bump 分配 */
    uint8_t* block = heap_ptr;
    if (block + total > heap_end) {
        caml_fatal_error_code(CAMELINO_ERR_OUT_OF_MEMORY, "heap exhausted (no GC yet)");
    }
    heap_ptr += total;
    heap_used_bytes += total;

    /* 写 header 在 block 起始 */
    header_t* hdr = (header_t*)block;
    *hdr = Make_header(wosize, tag, Caml_white);

    /* 返回值 = 指向 field0 的 value */
    value v = (value)(uintptr_t)(block + sizeof(header_t));

    return v;
}

value caml_alloc_small(mlsize_t wosize, tag_t tag) {
    return caml_alloc(wosize, tag);  /* Phase 3 小对象池优化 */
}

size_t caml_heap_used(void)  { return heap_used_bytes; }
size_t caml_heap_total(void) { return HEAP_SIZE; }

/* ---- 值栈 ---- */

static value* sp = NULL;

void caml_stack_init(void) {
    sp = camelino_stack + STACK_SIZE;   /* 高端（越过末尾），首次 push 前 --sp */
}

value* caml_stack_pointer(void) {
    return sp;
}

void caml_stack_push(value v) {
    if (sp <= camelino_stack) {
        caml_fatal_error_code(CAMELINO_ERR_STACK_OVERFLOW, "stack exhausted");
    }
    *(--sp) = v;
}

value caml_stack_pop(void) {
    if (sp >= camelino_stack + STACK_SIZE) {
        caml_fatal_error_code(CAMELINO_ERR_STACK_OVERFLOW, "stack underflow");
    }
    return *(sp++);
}

int caml_stack_check_overflow(void) {
    /* 留 guard zone：底部 8 slot */
    return (sp <= camelino_stack + 8) ? 1 : 0;
}

/* ---- 全局变量 ---- */

static mlsize_t globals_count = 0;

void caml_globals_init(void) {
    globals_count = 0;
}

value caml_global_get(mlsize_t slot) {
    if (slot >= globals_count) {
        caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "global slot out of range");
    }
    return camelino_globals[slot];
}

void caml_global_set(mlsize_t slot, value v) {
    if (slot >= MAX_GLOBALS) {
        caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "global slot exceeds MAX_GLOBALS");
    }
    camelino_globals[slot] = v;
    if (slot >= globals_count) {
        globals_count = slot + 1;
    }
}
/**
 * @file core/error.c
 * @brief 错误处理实现 — fatal 错误输出 + 断言
 */

#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void caml_fatal_error(const char* msg) {
    fprintf(stderr, "CAMELINO FATAL: %s\n", msg);
#ifdef CAMELINO_PLATFORM_ARDUINO
    /* Arduino: 串口输出后死循环 */
    while (1) {}
#else
    exit(1);
#endif
}

void caml_fatal_error_code(caml_err_t code, const char* detail) {
    fprintf(stderr, "CAMELINO FATAL [%d]: %s\n", (int)code, detail ? detail : "(no detail)");
#ifdef CAMELINO_PLATFORM_ARDUINO
    while (1) {}
#else
    exit(1);
#endif
}
/**
 * @file core/vm.c
 * @brief ZAM 虚拟机解释器 — caml_interpret() 主循环
 *
 * 实现 P0 指令集（约 40 条），与 OCaml 4.14 字节码兼容。
 * 编码以 runtime/interp.c 和 runtime/caml/instruct.h 为准。
 */
#include "vm.h"
#include "value.h"
#include "memory.h"
#include "error.h"
#include "opcodes.h"
#include <string.h>

/* ---- 全局 VM 状态 ---- */

static const uint8_t* code_start = NULL;   /* 字节码起始 */
static const uint8_t* pc = NULL;           /* 程序计数器 */
static const uint8_t* code_end = NULL;     /* 字节码末尾 */

static value accu = 0;                     /* 累加器 */
static value  env = 0;                     /* 当前环境（闭包） */
static int    extra_args = 0;              /* 剩余参数计数 */
static value* trap = NULL;                 /* 异常帧链表头 */
static int    halted = 0;                  /* STOP 后置 true */
static int    yield_counter = 0;           /* 指令计数，安全点 */

#define YIELD_INTERVAL 1000

/* primitive 表（Phase 4 实现） */
static value   prim_table_null(value a) { (void)a; return Val_unit; }
static value (*prim_table[256])(value) = { [0 ... 255] = prim_table_null };

/* ---- 辅助宏 ---- */

#define NEXT()     (void)0
#define NEXT_U32() ({ uint32_t v_ = CAML_READ_UINT32(pc); pc += 4; v_; })

/* ---- 初始化 ---- */

void caml_vm_init(void) {
    caml_memory_init();
    accu  = Val_unit;
    env   = Val_unit;
    extra_args = 0;
    trap  = NULL;
    halted = 0;
    yield_counter = YIELD_INTERVAL;
}

void caml_load_bytecode_buf(const uint8_t* code, size_t code_size,
                             const uint8_t* data, size_t data_size) {
    (void)data; (void)data_size;
    code_start = code;
    code_end   = code + code_size;
    pc = code_start;
    halted = 0;
}

/* ---- 解释器主循环 ---- */

void caml_interpret(void) {
    if (halted) return;
    if (!pc) return;

    for (;;) {
        uint8_t opcode = *pc++;

        switch (opcode) {

        /* ========== 常量 (P0) ========== */

        case CONST0:  accu = Val_long(0);  NEXT(); break;
        case CONST1:  accu = Val_long(1);  NEXT(); break;
        case CONST2:  accu = Val_long(2);  NEXT(); break;
        case CONST3:  accu = Val_long(3);  NEXT(); break;

        case CONSTINT:
            accu = Val_long(CAML_READ_INT32(pc));
            pc += 4;
            break;

        /* ========== 栈槽访问 (P0) ========== */

        case ACC0: accu = caml_stack_pointer()[0]; NEXT(); break;
        case ACC1: accu = caml_stack_pointer()[1]; NEXT(); break;
        case ACC2: accu = caml_stack_pointer()[2]; NEXT(); break;
        case ACC3: accu = caml_stack_pointer()[3]; NEXT(); break;
        case ACC4: accu = caml_stack_pointer()[4]; NEXT(); break;
        case ACC5: accu = caml_stack_pointer()[5]; NEXT(); break;
        case ACC6: accu = caml_stack_pointer()[6]; NEXT(); break;
        case ACC7: accu = caml_stack_pointer()[7]; NEXT(); break;

        case ACC: {
            uint8_t n = NEXT_U8();
            accu = caml_stack_pointer()[n];
            break;
        }

        case PUSH:
            caml_stack_push(accu);
            NEXT();
            break;

        case PUSHACC0:
            caml_stack_push(accu);
            accu = caml_stack_pointer()[1]; /* pushed acc is at sp[0], target at sp[1] */
            NEXT();
            break;
        case PUSHACC1:
            caml_stack_push(accu);
            accu = caml_stack_pointer()[2];
            NEXT();
            break;
        case PUSHACC2:
            caml_stack_push(accu);
            accu = caml_stack_pointer()[3];
            NEXT();
            break;
        case PUSHACC3:
            caml_stack_push(accu);
            accu = caml_stack_pointer()[4];
            NEXT();
            break;

        case PUSHACC: {
            uint8_t n = NEXT_U8();
            caml_stack_push(accu);
            accu = caml_stack_pointer()[n + 1]; /* +1 because we just pushed */
            break;
        }

        case POP: {
            uint16_t n = CAML_READ_UINT16(pc); pc += 2;
            value* s = caml_stack_pointer();
            s += n;
            (void)s; /* sp adjustment — just drop */
            /* Actually, we need to adjust sp properly */
            while (n--) caml_stack_pop();
            NEXT();
            break;
        }
        case ASSIGN: {
            uint8_t n = NEXT_U8();
            value* s = caml_stack_pointer();
            s[n] = accu;
            break;
        }

        /* ========== 整数运算 (P0) ========== */

        case ADDINT:
            /* tagged a + tagged b - 1 = tagged (a+b) */
            accu = (value)((intptr_t)accu + (intptr_t)caml_stack_pop() - 1);
            NEXT();
            break;
        case SUBINT:
            /* tagged a - tagged b + 1 = tagged (a-b) */
            accu = (value)((intptr_t)accu - (intptr_t)caml_stack_pop() + 1);
            NEXT();
            break;
        case MULINT:
            accu = Val_long(Long_val(accu) * Long_val(caml_stack_pop()));
            NEXT();
            break;
        case DIVINT: {
            intptr_t divisor = Long_val(caml_stack_pop());
            if (divisor == 0) { halted = 1; caml_fatal_error("Division_by_zero"); break; }
            accu = Val_long(Long_val(accu) / divisor);
            NEXT();
            break;
        }
        case MODINT: {
            intptr_t divisor = Long_val(caml_stack_pop());
            if (divisor == 0) { halted = 1; caml_fatal_error("Division_by_zero"); break; }
            accu = Val_long(Long_val(accu) % divisor);
            NEXT();
            break;
        }
        case NEGINT:
            accu = Val_long(-Long_val(accu));
            NEXT();
            break;

        case OFFSETINT: {
            /* accu += *pc << 1; pc++;  (operand is untagged, shifted to match tagged format) */
            int8_t offset = NEXT_S8();
            accu = (value)((intptr_t)accu + ((intptr_t)offset << 1));
            break;
        }

        /* 位运算 */
        case ANDINT:
            accu = Val_long(Long_val(accu) & Long_val(caml_stack_pop()));
            NEXT();
            break;
        case ORINT:
            accu = Val_long(Long_val(accu) | Long_val(caml_stack_pop()));
            NEXT();
            break;
        case XORINT:
            accu = Val_long(Long_val(accu) ^ Long_val(caml_stack_pop()));
            NEXT();
            break;
        case LSLINT:
            accu = Val_long(Long_val(accu) << Long_val(caml_stack_pop()));
            NEXT();
            break;
        case LSRINT: {
            uintptr_t v = (uintptr_t)Long_val(accu) >> Long_val(caml_stack_pop());
            accu = Val_long((intptr_t)v);
            NEXT();
            break;
        }
        case ASRINT:
            accu = Val_long(Long_val(accu) >> Long_val(caml_stack_pop()));
            NEXT();
            break;

        /* ========== 比较 (P0) ========== */

        case EQ:
            accu = Val_bool(Long_val(caml_stack_pop()) == Long_val(accu));
            NEXT();
            break;
        case NEQ:
            accu = Val_bool(Long_val(caml_stack_pop()) != Long_val(accu));
            NEXT();
            break;
        case LTINT:
            accu = Val_bool(Long_val(caml_stack_pop()) < Long_val(accu));
            NEXT();
            break;
        case LEINT:
            accu = Val_bool(Long_val(caml_stack_pop()) <= Long_val(accu));
            NEXT();
            break;
        case GTINT:
            accu = Val_bool(Long_val(caml_stack_pop()) > Long_val(accu));
            NEXT();
            break;
        case GEINT:
            accu = Val_bool(Long_val(caml_stack_pop()) >= Long_val(accu));
            NEXT();
            break;

        /* ========== 分支 (P0) ========== */

        case BRANCH: {
            int32_t offset = CAML_READ_INT32(pc);
            pc += offset;  /* relative to NEXT instruction? Check interp.c */
            /* Actually in OCaml: pc += offset where offset is from start of operand bytes */
            break;
        }
        case BRANCHIF: {
            int32_t offset = CAML_READ_INT32(pc);
            if (Long_val(accu) != 0) { pc += offset; }
            else { pc += 4; }
            break;
        }
        case BRANCHIFNOT: {
            int32_t offset = CAML_READ_INT32(pc);
            if (Long_val(accu) == 0) { pc += offset; }
            else { pc += 4; }
            break;
        }

        /* ========== 控制 (P0) ========== */

        case STOP:
            halted = 1;
            return;

        /* ========== 全局变量 (P0) ========== */

        case GETGLOBAL: {
            uint32_t slot = NEXT_U32();
            accu = caml_global_get((mlsize_t)slot);
            break;
        }
        case SETGLOBAL: {
            uint32_t slot = NEXT_U32();
            caml_global_set((mlsize_t)slot, accu);
            NEXT();
            break;
        }

        /* ATOM0: load atom (predefined constant) from global data */
        case ATOM0: {
            uint32_t index = NEXT_U32();
            (void)index;
            /* Phase 1: atom table not yet populated, accu stays as-is.
               In production, this loads from the global data table. */
            NEXT();
            break;
        }

        /* ========== 默认 ========== */

        default:
            fprintf(stderr, "VM: unimplemented opcode %d (0x%02x) at pc_off=%td\n",
                    opcode, opcode, pc - 1 - code_start);
            fflush(stderr);
            caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "unimplemented opcode");
            halted = 1;
            return;
            halted = 1;
            return;
        }

        /* 安全点：指令计数到达阈值 → yield */
        if (--yield_counter <= 0) {
            yield_counter = YIELD_INTERVAL;
            /* Phase 2: check yield flag + GC trigger here */
            return;  /* yield to main loop */
        }

        /* 栈溢出检查（Phase 2 实现异常后改为软检查） */
        if (caml_stack_check_overflow()) {
            caml_fatal_error_code(CAMELINO_ERR_STACK_OVERFLOW, "vm stack");
            halted = 1;
            return;
        }
    }
}

/* ---- 寄存器访问（测试用） ---- */

value caml_get_acc(void)       { return accu; }
void  caml_set_acc(value v)    { accu = v; }
value caml_get_sp(mlsize_t s)  { return caml_stack_pointer()[s]; }
value caml_get_global(mlsize_t slot) { return caml_global_get(slot); }
void  caml_set_global(mlsize_t slot, value v) { caml_global_set(slot, v); }
int   caml_vm_halted(void)     { return halted; }
#include "../../src/core/platform.h"
#include "../../src/core/camelino_config.h"
#include "../../src/core/value.h"
#include "../../src/core/vm.h"
#include "../../src/core/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HDR_SZ 32
int main(void){
 FILE*f=fopen("_t.camel","rb");fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);
 uint8_t*buf=malloc(len);fread(buf,1,len,f);fclose(f);
 uint32_t cs=(uint32_t)buf[20]|((uint32_t)buf[21]<<8)|((uint32_t)buf[22]<<16)|((uint32_t)buf[23]<<24);
 fprintf(stderr,"cs=%u code[0..%u]: ",cs,cs);
 for(uint32_t i=0;i<cs;i++)fprintf(stderr,"%02x ",buf[HDR_SZ+i]);
 fprintf(stderr,"\n");
 caml_vm_init();caml_load_bytecode_buf(buf+HDR_SZ,cs,NULL,0);
 caml_interpret();
 fprintf(stderr,"acc=%ld\n",(long)Long_val(caml_get_acc()));
 free(buf);return 0;
}
