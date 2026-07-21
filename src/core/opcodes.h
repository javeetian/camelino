/**
 * @file core/opcodes.h
 * @brief ZAM 字节码指令常量 — 与 OCaml 4.14 官方 instruct.h 逐字对齐
 *
 * 枚举值从 0 到 148，共 149 条指令。
 * 来源：ocaml/runtime/caml/instruct.h (OCaml 4.14.2)
 *
 * 实现优先级标注：
 *   P0 = Phase 1 必须（最小可运行：算术 + 调用 + 全局 + 堆块）
 *   P1 = Phase 5 补全（异常 + 浮点 + 字符串 + 循环）
 *   P2 = Phase 5 补全（对象 + 大偏移 + 剩余）
 */

#ifndef CAMELINO_OPCODES_H
#define CAMELINO_OPCODES_H

#include <stdint.h>
#include "byteorder.h"

/*==============================================================================
 * 指令枚举 — 与 OCaml 4.14 enum instructions 对齐
 *
 * C enum 从 0 开始自动递增，因此以下定义省去显式赋值。
 * 如果与 OCaml 版本不同，以 ocaml/runtime/caml/instruct.h 为准。
 *============================================================================*/

enum caml_instructions {
    /* ---- 栈槽访问 (P0) ---- */
    ACC0, ACC1, ACC2, ACC3, ACC4, ACC5, ACC6, ACC7,   /*  0–7   */
    ACC, PUSH,                                          /*  8–9   */
    PUSHACC0, PUSHACC1, PUSHACC2, PUSHACC3,             /* 10–13  */
    PUSHACC4, PUSHACC5, PUSHACC6, PUSHACC7,             /* 14–17  */
    PUSHACC, POP, ASSIGN,                               /* 18–20  */

    /* ---- 环境访问 (P0) ---- */
    ENVACC1, ENVACC2, ENVACC3, ENVACC4, ENVACC,         /* 21–25  */
    PUSHENVACC1, PUSHENVACC2, PUSHENVACC3, PUSHENVACC4, PUSHENVACC, /* 26–30 */

    /* ---- 函数调用 (P0) ---- */
    PUSH_RETADDR, APPLY, APPLY1, APPLY2, APPLY3,         /* 31–35  */
    APPTERM, APPTERM1, APPTERM2, APPTERM3,              /* 36–39  */
    RETURN, RESTART, GRAB,                               /* 40–42  */

    /* ---- 闭包 (P0) ---- */
    CLOSURE, CLOSUREREC,                                 /* 43–44  */

    /* ---- 闭包偏移访问 (P2) ---- */
    OFFSETCLOSUREM3, OFFSETCLOSURE0, OFFSETCLOSURE3, OFFSETCLOSURE, /* 45–48 */
    PUSHOFFSETCLOSUREM3, PUSHOFFSETCLOSURE0,             /* 49–50  */
    PUSHOFFSETCLOSURE3, PUSHOFFSETCLOSURE,               /* 51–52  */

    /* ---- 全局变量 (P0) ---- */
    GETGLOBAL, PUSHGETGLOBAL,                             /* 53–54  */
    GETGLOBALFIELD, PUSHGETGLOBALFIELD, SETGLOBAL,        /* 55–57  */

    /* ---- 原子常量 (P0, 预定义常量如 true/false/unit) ---- */
    ATOM0, ATOM, PUSHATOM0, PUSHATOM,                    /* 58–61  */

    /* ---- 堆块 (P0) ---- */
    MAKEBLOCK, MAKEBLOCK1, MAKEBLOCK2, MAKEBLOCK3, MAKEFLOATBLOCK, /* 62–66 */
    GETFIELD0, GETFIELD1, GETFIELD2, GETFIELD3, GETFIELD, GETFLOATFIELD, /* 67–72 */
    SETFIELD0, SETFIELD1, SETFIELD2, SETFIELD3, SETFIELD, SETFLOATFIELD, /* 73–78 */

    /* ---- 向量/数组 (P0) ---- */
    VECTLENGTH, GETVECTITEM, SETVECTITEM,                /* 79–81  */

    /* ---- 字节序列 (P2) ---- */
    GETBYTESCHAR, SETBYTESCHAR,                          /* 82–83  */

    /* ---- 分支 (P0) ---- */
    BRANCH, BRANCHIF, BRANCHIFNOT, SWITCH, BOOLNOT,      /* 84–88  */

    /* ---- 异常 (P1) ---- 注意：RAISE 也常用于 not_found 等控制流，Phase 1 不实现仍有大量 OCaml 程序无法工作 */
    PUSHTRAP, POPTRAP, RAISE,                            /* 89–91  */

    /* ---- 信号检查 (设备端桩) ---- */
    CHECK_SIGNALS,                                       /* 92     */

    /* ---- C FFI (P0, Phase 4 实现) ---- */
    C_CALL1, C_CALL2, C_CALL3, C_CALL4, C_CALL5, C_CALLN, /* 93–98 */

    /* ---- 常量 (P0) ---- */
    CONST0, CONST1, CONST2, CONST3, CONSTINT,             /* 99–103 */
    PUSHCONST0, PUSHCONST1, PUSHCONST2, PUSHCONST3, PUSHCONSTINT, /* 104–108 */

    /* ---- 整数运算 (P0) ---- */
    NEGINT, ADDINT, SUBINT, MULINT, DIVINT, MODINT,       /* 109–114 */
    ANDINT, ORINT, XORINT, LSLINT, LSRINT, ASRINT,        /* 115–120 */

    /* ---- 比较 (P0) ---- */
    EQ, NEQ, LTINT, LEINT, GTINT, GEINT,                   /* 121–126 */

    /* ---- 整数/ref 操作 (P1) ---- */
    OFFSETINT, OFFSETREF, ISINT,                           /* 127–129 */

    /* ---- 对象方法 (P2) ---- */
    GETMETHOD,                                             /* 130     */

    /* ---- 分支比较 (P1) ---- */
    BEQ, BNEQ, BLTINT, BLEINT, BGTINT, BGEINT,             /* 131–136 */

    /* ---- 无符号比较 (P1) ---- */
    ULTINT, UGEINT,                                        /* 137–138 */
    BULTINT, BUGEINT,                                      /* 139–140 */

    /* ---- 对象 (P2) ---- */
    GETPUBMET, GETDYNMET,                                  /* 141–142 */

    /* ---- 控制 / 调试 (P1) ---- */
    STOP, EVENT, BREAK,                                    /* 143–145 */

    /* ---- 异常扩展 (P1) ---- */
    RERAISE, RAISE_NOTRACE,                                /* 146–147 */

    /* ---- 字符串 (P2) ---- */
    GETSTRINGCHAR,                                         /* 148     */

    CAML_NUM_INSTRUCTIONS
};

/*==============================================================================
 * 操作数解码辅助宏
 *
 * pc 是指向当前字节码指令的 uint8_t* 指针，指令本身占用 1 字节(opcode)。
 * 操作数紧跟其后，大端/小端已由 camelino-embed 规范化（§3.4.2），
 * 设备端通过 CAML_READ_* 读取。
 *
 * 这些宏在 vm.c 解释循环中使用，语义为"取操作数并推进 pc"。
 *============================================================================*/

/* 取 1 字节无符号操作数（全局 pc 变量，定义在 vm.c 中） */
#define NEXT_U8()   (*((pc)++))

/* 取 1 字节有符号操作数 */
#define NEXT_S8()   ((int8_t)(*((pc)++)))

/* 取 2 字节无符号操作数（大端/小端已由 byteorder.h 处理） */
static inline uint16_t NEXT_U16(const uint8_t** ppc) {
    uint16_t v = CAML_READ_UINT16(*ppc);
    *ppc += 2;
    return v;
}

/* 取 4 字节有符号操作数（用于 CONSTINT、分支长偏移） */
static inline int32_t NEXT_S32(const uint8_t** ppc) {
    int32_t v = CAML_READ_INT32(*ppc);
    *ppc += 4;
    return v;
}

/* 取 4 字节无符号操作数（用于 GETGLOBAL slot、大分支偏移等） */
static inline uint32_t NEXT_U32(const uint8_t** ppc) {
    uint32_t v = CAML_READ_UINT32(*ppc);
    *ppc += 4;
    return v;
}

/*==============================================================================
 * 指令优先级分类（注释用，供实现者参考）
 *============================================================================*/

/* P0 — Phase 1 必须实现 ≈ 40 条核心指令 */
#define CAML_OP_P0  0

/* P1 — Phase 5 补全（异常、浮点、字符串、循环） ≈ 30 条 */
#define CAML_OP_P1  1

/* P2 — Phase 5 补全（对象、大偏移、剩余指令） ≈ 79 条 */
#define CAML_OP_P2  2

#endif /* CAMELINO_OPCODES_H */
