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

/* FFI 调度 (ffi.c) */
typedef value (*caml_prim_t)(value);
typedef value (*caml_prim2_t)(value, value);
typedef value (*caml_prim3_t)(value, value, value);
extern void* caml_lookup_primitive_by_index(int index, int* arity);

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

/* ---- C_CALL* 调度 ---- */
static value prim_dispatch(int idx, value* args, int nargs) {
    int arity; caml_prim_t f = (caml_prim_t)caml_lookup_primitive_by_index(idx, &arity);
    if (!f) return Val_unit;
    switch (nargs) {
        case 0: return f(Val_unit);
        case 1: return f(args[0]);
        case 2: return ((caml_prim2_t)f)(args[0], args[1]);
        case 3: return ((caml_prim3_t)f)(args[0], args[1], args[2]);
        default: return Val_unit;
    }
}

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

void caml_startup(const uint8_t* code, size_t code_size) {
    caml_vm_init();
    caml_load_bytecode_buf(code, code_size, NULL, 0);
    caml_init_globals();
}

void caml_init_globals(void) {
    /* Phase 2 stub: global data is pre-initialized by camelino-embed.
       For simple programs, SETGLOBAL in the bytecode handles initialization.
       For multi-CU programs, this iterates CU init code. */
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

        case PUSHCONSTINT:
            caml_stack_push(accu);
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

        /* ========== 函数调用 (Phase 2) ========== */

        case PUSH_RETADDR: {
            int32_t ret_offset = CAML_READ_INT32(pc);
            pc += 4;
            caml_stack_push(Val_long(extra_args));
            caml_stack_push(env);
            caml_stack_push((value)(uintptr_t)(pc + ret_offset));
            NEXT();
            break;
        }

        case APPLY: {
            uint8_t nargs = NEXT_U8();
            value clos = accu;
            value saved_env = env;
            env = clos;
            extra_args = (int)nargs - 1;
            caml_stack_push(Val_long(extra_args));
            caml_stack_push(saved_env);
            caml_stack_push((value)(uintptr_t)pc);
            size_t off = (size_t)Long_val(Field(clos, 0));
            if (off < (size_t)(code_end - code_start))
                pc = code_start + off;
            else
                pc = code_start;
            NEXT();
            break;
        }

        case APPLY1: {
            value clos = accu;
            value saved_env = env;
            env = clos;
            extra_args = 0;
            caml_stack_push(Val_long(0));
            caml_stack_push(saved_env);
            caml_stack_push((value)(uintptr_t)pc);
            size_t off = (size_t)Long_val(Field(clos, 0));
            if (off < (size_t)(code_end - code_start))
                pc = code_start + off;
            else
                pc = code_start;
            NEXT();
            break;
        }

        case APPLY2: {
            value clos = accu;
            value saved_env = env;
            env = clos;
            extra_args = 1;
            caml_stack_push(Val_long(1));
            caml_stack_push(saved_env);
            caml_stack_push((value)(uintptr_t)pc);
            size_t off = (size_t)Long_val(Field(clos, 0));
            if (off < (size_t)(code_end - code_start))
                pc = code_start + off;
            else
                pc = code_start;
            NEXT();
            break;
        }

        case APPLY3: {
            value clos = accu;
            value saved_env = env;
            env = clos;
            extra_args = 2;
            caml_stack_push(Val_long(2));
            caml_stack_push(saved_env);
            caml_stack_push((value)(uintptr_t)pc);
            size_t off = (size_t)Long_val(Field(clos, 0));
            if (off < (size_t)(code_end - code_start))
                pc = code_start + off;
            else
                pc = code_start;
            NEXT();
            break;
        }

        case GRAB: {
            uint8_t required = NEXT_U8();
            if (extra_args >= required) {
                /* enough args, continue */
            } else {
                /* not enough args: create partial application (Phase 2.3+) */
                /* For now, fatal if insufficient */
                halted = 1; return;
            }
            NEXT();
            break;
        }

        case RETURN: {
            uint8_t n = NEXT_U8();
            value saved_pc    = caml_stack_pop();
            value saved_env   = caml_stack_pop();
            value saved_extra = caml_stack_pop();
            extra_args = (int)Long_val(saved_extra);
            env = saved_env;
            pc = (const uint8_t*)(uintptr_t)saved_pc;
            while (n--) caml_stack_pop();
            NEXT();
            break;
        }

        case RESTART: {
            /* Reorganize stack for multi-arg application */
            /* Phase 2.3: implement full RESTART */
            NEXT();
            break;
        }

        case APPTERM:
        case APPTERM1:
        case APPTERM2:
        case APPTERM3: {
            /* Tail call: Phase 2.3 */
            halted = 1; return;
        }

        case CLOSURE: {
            /* 4.14 closure: field0=code_ptr, field1=closinfo, field2..=free vars */
            int32_t code_off = CAML_READ_INT32(pc);
            pc += 4;
            mlsize_t nvars = NEXT_U8();
            /* closinfo: tagged arity from the bytecode */
            uint8_t arity = NEXT_U8();
            value clos = caml_alloc(2 + nvars, Closure_tag);  /* 2 + nvars fields */
            Field(clos, 0) = Val_long(code_off);
            Field(clos, 1) = Val_long((intptr_t)arity);       /* closinfo = tagged arity */
            for (mlsize_t i = 0; i < nvars; i++) {
                Field(clos, 2 + i) = caml_stack_pop();
            }
            accu = clos;
            NEXT();
            break;
        }

        case CLOSUREREC: {
            /* Recursive closure: Phase 2.3 */
            halted = 1; return;
        }

        /* ========== 异常 (Phase 2.3) ========== */

        case PUSHTRAP: {
            int32_t off = CAML_READ_INT32(pc); pc += 4;
            caml_stack_push(accu);
            caml_stack_push(Val_long(extra_args));
            caml_stack_push(env);
            caml_stack_push((value)(uintptr_t)(pc + off));
            trap = caml_stack_pointer();  /* points to handler_pc slot */
            NEXT(); break;
        }

        case POPTRAP: {
            /* skip 4 slots back to where trap was pushed */
            value* sp = caml_stack_pointer();
            accu       = sp[3];  /* restored accu */
            extra_args = (int)Long_val(sp[2]);
            env        = sp[1];
            (void)sp[0];        /* discard handler_pc */
            /* pop 4 slots */
            caml_stack_pop(); caml_stack_pop(); caml_stack_pop(); caml_stack_pop();
            trap = NULL;  /* no more trap */
            NEXT(); break;
        }

        case RAISE:
        case RAISE_NOTRACE: {
            if (trap == NULL) { halted = 1; return; }
            /* trap points to handler_pc on the stack */
            value* tr = trap;
            pc        = (const uint8_t*)(uintptr_t)tr[0];  /* handler_pc */
            env       = tr[1];
            extra_args = (int)Long_val(tr[2]);
            /* accu KEEPS current value (the exception) — do NOT restore tr[3] */
            /* unwind the stack: pop everything down to trap frame */
            value* sp = caml_stack_pointer();
            while (sp > tr) { caml_stack_pop(); sp = caml_stack_pointer(); }
            /* pop the 4 trap slots */
            caml_stack_pop(); caml_stack_pop(); caml_stack_pop(); caml_stack_pop();
            trap = NULL;  /* trap consumed */
            NEXT(); break;
        }

        /* ========== 控制 (P0) ========== */

        case STOP:
            halted = 1;
            return;

        /* ========== C primitive 调用 (Phase 4) ========== */

        case C_CALL1: {
            int idx = (int)NEXT_U32();
            value a = caml_stack_pop();
            accu = prim_dispatch(idx, &a, 1);
            NEXT(); break;
        }
        case C_CALL2: {
            int idx = (int)NEXT_U32();
            value a2 = caml_stack_pop(), a1 = caml_stack_pop();
            value args[2] = { a1, a2 };
            accu = prim_dispatch(idx, args, 2);
            NEXT(); break;
        }
        case C_CALL3: {
            int idx = (int)NEXT_U32();
            value a3 = caml_stack_pop(), a2 = caml_stack_pop(), a1 = caml_stack_pop();
            value args[3] = { a1, a2, a3 };
            accu = prim_dispatch(idx, args, 3);
            NEXT(); break;
        }
        case C_CALL4:
        case C_CALL5:
        case C_CALLN: {
            int idx = (int)NEXT_U32();
            int n = NEXT_U8(); /* nargs for C_CALLN */
            value args[8] = { Val_unit };
            for (int i = n-1; i >= 0; i--) args[i] = caml_stack_pop();
            accu = prim_dispatch(idx, args, n);
            NEXT(); break;
        }
        case CHECK_SIGNALS:
            /* no-op on embedded */
            NEXT(); break;

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
        case GETGLOBALFIELD: {
            uint32_t slot = NEXT_U32();
            uint8_t  idx  = NEXT_U8();
            value g = caml_global_get((mlsize_t)slot);
            accu = Field(g, (mlsize_t)idx);
            NEXT();
            break;
        }
        case PUSHGETGLOBAL: {
            uint32_t slot = NEXT_U32();
            caml_stack_push(accu);          /* save current acc */
            accu = caml_global_get((mlsize_t)slot);
            break;
        }
        case PUSHGETGLOBALFIELD: {
            uint32_t slot = NEXT_U32();
            uint8_t  idx  = NEXT_U8();
            value g = caml_global_get((mlsize_t)slot);
            caml_stack_push(accu);
            accu = Field(g, (mlsize_t)idx);
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
value caml_get_env(void)       { return env; }
value* caml_get_trap_ptr(void) { return trap; }
value caml_get_sp(mlsize_t s)  { return caml_stack_pointer()[s]; }
value caml_get_global(mlsize_t slot) { return caml_global_get(slot); }
void  caml_set_global(mlsize_t slot, value v) { caml_global_set(slot, v); }
int   caml_vm_halted(void)     { return halted; }

/* ---- C → OCaml 回调 (4.5) ---- */

value caml_callback(value closure, value arg) {
    value saved_env = env;
    value saved_acc = accu;
    int  saved_ea  = extra_args;

    /* Push arg + return frame, call closure */
    caml_stack_push(arg);
    env = closure;
    extra_args = 0;
    caml_stack_push(Val_long(0));
    caml_stack_push(saved_env);
    caml_stack_push((value)(uintptr_t)pc);

    /* Update pc to closure code */
    size_t off = (size_t)Long_val(Field(closure, 0));
    if (off < (size_t)(code_end - code_start))
        pc = code_start + off;

    /* Run until RETURN restores pc to saved value */
    caml_interpret();

    value result = accu;
    accu = saved_acc;
    env = saved_env;
    extra_args = saved_ea;
    return result;
}

value caml_callback2(value closure, value a1, value a2) {
    caml_stack_push(a1);
    caml_stack_push(a2);
    return caml_callback(closure, a2);
}

/* ---- 事件处理 / yield (4.6) ---- */

void caml_process_events(void) {
    /* Phase 4: 轮询 ISR flag，当前 stub */
}
