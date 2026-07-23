/**
 * examples/host/AllOpcodes/test_all.c
 * @brief 全指令快速测试 — 已验证的指令类别
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/core/platform.h"
#include "../../src/core/value.h"
#include "../../src/core/memory.h"
#include "../../src/core/vm.h"
#include "../../src/core/opcodes.h"
#include "../../src/ffi/ffi.h"

static int run=0, pass=0, fail=0, cp=0, ct=0;

static int64_t exec(const uint8_t* c, size_t n) {
    caml_vm_init();
    caml_load_bytecode_buf(c, n, NULL, 0);
    caml_interpret();
    return Long_val(caml_get_acc());
}
static void T(const char* nm, const uint8_t* c, size_t n, int64_t e) {
    run++;ct++;int64_t a=exec(c,n);
    if(a==e){pass++;cp++;}else{fail++;printf("  FAIL %-42s e=%lld a=%lld\n",nm,(long long)e,(long long)a);}
}
static void w32(uint8_t*p,int32_t v){p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;}
static void w32u(uint8_t*p,uint32_t v){p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;}
#define CAT(n) do{printf("\n── %s ──\n",n);cp=ct=0;}while(0)
#define DONE() do{printf("  %d/%d passed\n",cp,ct);}while(0)

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║   Camelino Full Opcode Test                             ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    CAT("Constants (CONST0-3, CONSTINT, PUSHCONSTINT)");
    {uint8_t c[]={CONST0,STOP};T("CONST0",c,2,0);}
    {uint8_t c[]={CONST1,STOP};T("CONST1",c,2,1);}
    {uint8_t c[]={CONST2,STOP};T("CONST2",c,2,2);}
    {uint8_t c[]={CONST3,STOP};T("CONST3",c,2,3);}
    {uint8_t c[6]={CONSTINT};w32(c+1,42);c[5]=STOP;T("CONSTINT(42)",c,6,42);}
    {uint8_t c[6]={CONSTINT};w32(c+1,-99);c[5]=STOP;T("CONSTINT(-99)",c,6,-99);}
    {uint8_t c[6]={CONSTINT};w32(c+1,12345);c[5]=STOP;T("CONSTINT(12345)",c,6,12345);}
    {uint8_t c[6]={CONSTINT};w32(c+1,0);c[5]=STOP;T("CONSTINT(0)",c,6,0);}
    {uint8_t c[6]={PUSHCONSTINT};w32(c+1,77);c[5]=STOP;T("PUSHCONSTINT(77)",c,6,77);}
    {uint8_t c[]={PUSHCONST0,STOP};T("PUSHCONST0",c,2,0);}
    {uint8_t c[]={PUSHCONST1,STOP};T("PUSHCONST1",c,2,1);}
    {uint8_t c[]={PUSHCONST2,STOP};T("PUSHCONST2",c,2,2);}
    {uint8_t c[]={PUSHCONST3,STOP};T("PUSHCONST3",c,2,3);}
    DONE();

    CAT("Arithmetic (ADD,SUB,MUL,DIV,MOD,NEG,AND,OR,XOR,LSL,LSR,ASR)");
    {uint8_t c[]={CONST3,PUSH,CONST2,ADDINT,STOP};T("ADD 3+2→5",c,5,5);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,10);c[7]=SUBINT;c[8]=STOP;T("SUB 10-3→7",c,9,7);}
    {uint8_t c[13]={CONSTINT};w32(c+1,7);c[5]=PUSH;c[6]=CONSTINT;w32(c+7,6);c[11]=MULINT;c[12]=STOP;T("MUL 6*7→42",c,13,42);}
    {uint8_t c[9]={CONST2,PUSH,CONSTINT};w32(c+3,6);c[7]=DIVINT;c[8]=STOP;T("DIV 6/2→3",c,9,3);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,10);c[7]=MODINT;c[8]=STOP;T("MOD 10%3→1",c,9,1);}
    {uint8_t c[7]={CONSTINT};w32(c+1,42);c[5]=NEGINT;c[6]=STOP;T("NEG 42→-42",c,7,-42);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,5);c[7]=ANDINT;c[8]=STOP;T("AND 5&3→1",c,9,1);}
    {uint8_t c[9]={CONST2,PUSH,CONSTINT};w32(c+3,5);c[7]=ORINT;c[8]=STOP;T("OR 5|2→7",c,9,7);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,5);c[7]=XORINT;c[8]=STOP;T("XOR 5^3→6",c,9,6);}
    {uint8_t c[5]={CONST3,PUSH,CONST1,LSLINT,STOP};T("LSL 1<<3→8",c,5,8);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,8);c[7]=LSRINT;c[8]=STOP;T("LSR 8>>3→1",c,9,1);}
    DONE();

    CAT("Offset / Int (OFFSETINT, ISINT, BOOLNOT)");
    {uint8_t c[]={CONST2,OFFSETINT,3,STOP};T("OFFSETINT +3→5",c,4,5);}
    {uint8_t c[]={CONST3,ISINT,STOP};T("ISINT 3→true",c,3,1);}
    {uint8_t c[]={CONST0,BOOLNOT,STOP};T("BOOLNOT false→true",c,3,1);}
    {uint8_t c[]={CONST1,BOOLNOT,STOP};T("BOOLNOT true→false",c,3,0);}
    DONE();

    CAT("Comparison (EQ,NEQ,LTINT,LEINT,GTINT,GEINT,ULTINT,UGEINT)");
    {uint8_t c[]={CONST3,PUSH,CONST3,EQ,STOP};T("EQ 3==3→true",c,5,1);}
    {uint8_t c[9]={CONSTINT};w32(c+1,5);c[5]=PUSH;c[6]=CONST3;c[7]=EQ;c[8]=STOP;T("EQ 3==5→false",c,9,0);}
    {uint8_t c[9]={CONSTINT};w32(c+1,5);c[5]=PUSH;c[6]=CONST3;c[7]=NEQ;c[8]=STOP;T("NEQ 3!=5→true",c,9,1);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,5);c[7]=LTINT;c[8]=STOP;T("LT 3<5→true",c,9,1);}
    {uint8_t c[9]={CONSTINT};w32(c+1,5);c[5]=PUSH;c[6]=CONST3;c[7]=LTINT;c[8]=STOP;T("LT 5<3→false",c,9,0);}
    {uint8_t c[]={CONST3,PUSH,CONST3,LEINT,STOP};T("LE 3<=3→true",c,5,1);}
    {uint8_t c[9]={CONST3,PUSH,CONSTINT};w32(c+3,5);c[7]=GTINT;c[8]=STOP;T("GT 3>5→false",c,9,0);}
    {uint8_t c[9]={CONSTINT};w32(c+1,5);c[5]=PUSH;c[6]=CONST3;c[7]=GTINT;c[8]=STOP;T("GT 5>3→true",c,9,1);}
    {uint8_t c[]={CONST3,PUSH,CONST3,GEINT,STOP};T("GE 3>=3→true",c,5,1);}
    {uint8_t c[9]={CONSTINT,255,255,255,255,PUSH,CONST1,ULTINT,STOP};T("ULTINT -1u<1u→false",c,9,0);}
    {uint8_t c[9]={CONSTINT,255,255,255,255,PUSH,CONST1,UGEINT,STOP};T("UGEINT -1u>=1u→true",c,9,1);}
    DONE();

    CAT("Globals (GETGLOBAL, SETGLOBAL)");
    {uint8_t c[16]={CONSTINT};w32(c+1,42);c[5]=SETGLOBAL;w32u(c+6,0);c[10]=GETGLOBAL;w32u(c+11,0);c[15]=STOP;
     T("SET+GET global→42",c,16,42);}
    {uint8_t c[16]={CONSTINT};w32(c+1,7);c[5]=SETGLOBAL;w32u(c+6,1);c[10]=GETGLOBAL;w32u(c+11,1);c[15]=STOP;
     T("SET+GET slot 1→7",c,16,7);}
    DONE();

    CAT("Heap Blocks (MAKEBLOCK1, GETFIELD0, GETFIELD, VECTLENGTH)");
    {uint8_t c[11]={CONSTINT};w32(c+1,42);c[5]=MAKEBLOCK1;c[6]=0;c[7]=1;c[8]=0;c[9]=GETFIELD0;c[10]=STOP;
     T("MAKEBLOCK1+GETFIELD0→42",c,11,42);}
    DONE();

    CAT("Exception (uncaught RAISE → halt)");
    {uint8_t c[]={CONST3,RAISE,CONST0,STOP};
     caml_vm_init();caml_load_bytecode_buf(c,sizeof(c),NULL,0);caml_interpret();
     run++;ct++;if(caml_vm_halted()){pass++;cp++;}else{fail++;printf("  FAIL uncaught RAISE\n");};}
    DONE();

    CAT("FFI (C_CALL1, C_CALL2)");
    {uint8_t c[11]={CONSTINT};w32(c+1,42);c[5]=PUSH;c[6]=C_CALL1;w32u(c+7,0);c[11]=STOP;
     caml_vm_init();caml_init_primitives();caml_load_bytecode_buf(c,sizeof(c),NULL,0);caml_interpret();
     run++;ct++;int64_t v=Long_val(caml_get_acc());
     if(v==42){pass++;cp++;}else{fail++;printf("  FAIL C_CALL1 identity e=42 a=%lld\n",(long long)v);};}
    {uint8_t c[16]={CONSTINT};w32(c+1,41);c[5]=PUSH;c[6]=CONSTINT;w32(c+7,1);c[11]=PUSH;
     c[12]=C_CALL2;w32u(c+13,1);c[17]=STOP;
     caml_vm_init();caml_init_primitives();caml_load_bytecode_buf(c,sizeof(c),NULL,0);caml_interpret();
     run++;ct++;int64_t v2=Long_val(caml_get_acc());
     if(v2==42){pass++;cp++;}else{fail++;printf("  FAIL C_CALL2 add1 e=42 a=%lld\n",(long long)v2);};}
    DONE();

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  %d passed / %d failed (out of %d tests)\n", pass, fail, run);
    printf("═══════════════════════════════════════════════════════════\n");
    return fail > 0 ? 1 : 0;
}
