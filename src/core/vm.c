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

#define NEXT()  (void)0

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

        /* ========== 默认 ========== */

        default:
            /* 未实现的指令：打印错误并停机（开发期友好） */
            caml_fatal_error_code(CAMELINO_ERR_INTERNAL, "unimplemented opcode");
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
