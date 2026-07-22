/**
 * @file core/vm.c
 * @brief ZAM 虚拟机 — P0+P1 指令解释器 (Phase 1-5)
 */
#include "vm.h"
#include "value.h"
#include "memory.h"
#include "error.h"
#include "opcodes.h"
#include <string.h>

typedef value (*caml_prim_t)(value);
typedef value (*caml_prim2_t)(value, value);
typedef value (*caml_prim3_t)(value, value, value);
#ifdef CAMELINO_HAS_FFI
extern void* caml_lookup_primitive_by_index(int index, int* arity);
#else
__attribute__((weak))
void* caml_lookup_primitive_by_index(int index, int* arity) {
    (void)index;
    if (arity) *arity = 0;
    return NULL;
}
#endif

static const uint8_t* code_start = NULL;
static const uint8_t* pc = NULL;
static const uint8_t* code_end = NULL;
static value accu = 0, env_v = 0;
static int extra_args = 0, halted = 0, yield_counter = 0;
static value* trap = NULL;
static size_t instruction_count = 0;
static caml_trace_fn trace_hook = NULL;
#define YIELD_INTERVAL 1000

#define NEXT() (void)0
#define NEXT_U32() ({uint32_t v_=CAML_READ_UINT32(pc);pc+=4;v_;})

static value p_dispatch(int idx, value* args, int nargs) {
    int arity; caml_prim_t f=(caml_prim_t)caml_lookup_primitive_by_index(idx,&arity);
    if(!f)return Val_unit;
    switch(nargs){case 0:return f(Val_unit);case 1:return f(args[0]);
    case 2:return((caml_prim2_t)f)(args[0],args[1]);
    case 3:return((caml_prim3_t)f)(args[0],args[1],args[2]);default:return Val_unit;}
}

void caml_vm_init(void){caml_memory_init();accu=Val_unit;env_v=Val_unit;extra_args=0;trap=NULL;halted=0;yield_counter=YIELD_INTERVAL;instruction_count=0;}
void caml_startup(const uint8_t* c,size_t s){caml_vm_init();caml_load_bytecode_buf(c,s,NULL,0);caml_init_globals();}
void caml_init_globals(void){}
void caml_load_bytecode_buf(const uint8_t* c,size_t cs,const uint8_t*d,size_t ds){(void)d;(void)ds;code_start=c;code_end=c+cs;pc=code_start;halted=0;}

void caml_interpret(void){
    if(halted||!pc)return;
    for(;;){
        uint8_t op=*pc++;
        instruction_count++;
        if(trace_hook)trace_hook(op,(size_t)(pc-1-code_start),accu);
        switch(op){
        case CONST0:accu=Val_long(0);NEXT();break;
        case CONST1:accu=Val_long(1);NEXT();break;
        case CONST2:accu=Val_long(2);NEXT();break;
        case CONST3:accu=Val_long(3);NEXT();break;
        case CONSTINT:accu=Val_long(CAML_READ_INT32(pc));pc+=4;break;
        case PUSHCONSTINT:caml_stack_push(accu);accu=Val_long(CAML_READ_INT32(pc));pc+=4;break;

        case ACC0:accu=caml_stack_pointer()[0];NEXT();break;
        case ACC1:accu=caml_stack_pointer()[1];NEXT();break;
        case ACC2:accu=caml_stack_pointer()[2];NEXT();break;
        case ACC3:accu=caml_stack_pointer()[3];NEXT();break;
        case ACC4:accu=caml_stack_pointer()[4];NEXT();break;
        case ACC5:accu=caml_stack_pointer()[5];NEXT();break;
        case ACC6:accu=caml_stack_pointer()[6];NEXT();break;
        case ACC7:accu=caml_stack_pointer()[7];NEXT();break;
        case ACC:{uint8_t n=NEXT_U8();accu=caml_stack_pointer()[n];break;}
        case PUSH:caml_stack_push(accu);NEXT();break;
        case PUSHACC0:caml_stack_push(accu);accu=caml_stack_pointer()[1];NEXT();break;
        case PUSHACC1:caml_stack_push(accu);accu=caml_stack_pointer()[2];NEXT();break;
        case PUSHACC2:caml_stack_push(accu);accu=caml_stack_pointer()[3];NEXT();break;
        case PUSHACC3:caml_stack_push(accu);accu=caml_stack_pointer()[4];NEXT();break;
        case PUSHACC4:caml_stack_push(accu);accu=caml_stack_pointer()[5];NEXT();break;
        case PUSHACC5:caml_stack_push(accu);accu=caml_stack_pointer()[6];NEXT();break;
        case PUSHACC6:caml_stack_push(accu);accu=caml_stack_pointer()[7];NEXT();break;
        case PUSHACC7:caml_stack_push(accu);accu=caml_stack_pointer()[8];NEXT();break;
        case PUSHACC:{uint8_t n=NEXT_U8();caml_stack_push(accu);accu=caml_stack_pointer()[n+1];break;}
        case POP:{uint16_t n=CAML_READ_UINT16(pc);pc+=2;while(n--)caml_stack_pop();NEXT();break;}
        case ASSIGN:{uint8_t n=NEXT_U8();caml_stack_pointer()[n]=accu;break;}

        case ADDINT:accu=(value)((intptr_t)accu+(intptr_t)caml_stack_pop()-1);NEXT();break;
        case SUBINT:accu=(value)((intptr_t)accu-(intptr_t)caml_stack_pop()+1);NEXT();break;
        case MULINT:accu=Val_long(Long_val(accu)*Long_val(caml_stack_pop()));NEXT();break;
        case DIVINT:{intptr_t d=Long_val(caml_stack_pop());if(!d){halted=1;return;}accu=Val_long(Long_val(accu)/d);NEXT();break;}
        case MODINT:{intptr_t d=Long_val(caml_stack_pop());if(!d){halted=1;return;}accu=Val_long(Long_val(accu)%d);NEXT();break;}
        case NEGINT:accu=Val_long(-Long_val(accu));NEXT();break;
        case OFFSETINT:{int8_t o=NEXT_S8();accu=(value)((intptr_t)accu+((intptr_t)o<<1));break;}
        case ANDINT:accu=Val_long(Long_val(accu)&Long_val(caml_stack_pop()));NEXT();break;
        case ORINT:accu=Val_long(Long_val(accu)|Long_val(caml_stack_pop()));NEXT();break;
        case XORINT:accu=Val_long(Long_val(accu)^Long_val(caml_stack_pop()));NEXT();break;
        case LSLINT:accu=Val_long(Long_val(accu)<<Long_val(caml_stack_pop()));NEXT();break;
        case LSRINT:{uintptr_t v=(uintptr_t)Long_val(accu)>>Long_val(caml_stack_pop());accu=Val_long((intptr_t)v);NEXT();break;}
        case ASRINT:accu=Val_long(Long_val(accu)>>Long_val(caml_stack_pop()));NEXT();break;

        case EQ:accu=Val_bool(Long_val(caml_stack_pop())==Long_val(accu));NEXT();break;
        case NEQ:accu=Val_bool(Long_val(caml_stack_pop())!=Long_val(accu));NEXT();break;
        case LTINT:accu=Val_bool(Long_val(caml_stack_pop())<Long_val(accu));NEXT();break;
        case LEINT:accu=Val_bool(Long_val(caml_stack_pop())<=Long_val(accu));NEXT();break;
        case GTINT:accu=Val_bool(Long_val(caml_stack_pop())>Long_val(accu));NEXT();break;
        case GEINT:accu=Val_bool(Long_val(caml_stack_pop())>=Long_val(accu));NEXT();break;

        case BEQ:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())==Long_val(accu))pc+=o;else pc+=4;break;}
        case BNEQ:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())!=Long_val(accu))pc+=o;else pc+=4;break;}
        case BLTINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())<Long_val(accu))pc+=o;else pc+=4;break;}
        case BLEINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())<=Long_val(accu))pc+=o;else pc+=4;break;}
        case BGTINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())>Long_val(accu))pc+=o;else pc+=4;break;}
        case BGEINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())>=Long_val(accu))pc+=o;else pc+=4;break;}
        case ULTINT:accu=Val_bool((uintptr_t)Long_val(caml_stack_pop())<(uintptr_t)Long_val(accu));NEXT();break;
        case UGEINT:accu=Val_bool((uintptr_t)Long_val(caml_stack_pop())>=(uintptr_t)Long_val(accu));NEXT();break;
        case BOOLNOT:accu=Val_bool(Long_val(accu)==0);NEXT();break;

        case OFFSETREF:{int8_t n=NEXT_S8();Field(accu,0)=Val_long(Long_val(Field(accu,0))+n);NEXT();break;}
        case ISINT:accu=Val_bool(Is_long(accu));NEXT();break;

        case BRANCH:{int32_t o=CAML_READ_INT32(pc);pc+=o;break;}
        case BRANCHIF:{int32_t o=CAML_READ_INT32(pc);if(Long_val(accu)!=0)pc+=o;else pc+=4;break;}
        case BRANCHIFNOT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(accu)==0)pc+=o;else pc+=4;break;}

        case PUSHTRAP:{int32_t o=CAML_READ_INT32(pc);pc+=4;caml_stack_push(accu);caml_stack_push(Val_long(extra_args));caml_stack_push(env_v);caml_stack_push((value)(uintptr_t)(pc+o));trap=caml_stack_pointer();NEXT();break;}
        case POPTRAP:{value*sp=caml_stack_pointer();accu=sp[3];extra_args=(int)Long_val(sp[2]);env_v=sp[1];caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}
        case RAISE:case RAISE_NOTRACE:{if(!trap){halted=1;return;}value*tr=trap;pc=(const uint8_t*)(uintptr_t)tr[0];env_v=tr[1];extra_args=(int)Long_val(tr[2]);while(caml_stack_pointer()>tr)caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}
        case RERAISE:{if(!trap){halted=1;return;}value*tr=trap;pc=(const uint8_t*)(uintptr_t)tr[0];env_v=tr[1];extra_args=(int)Long_val(tr[2]);while(caml_stack_pointer()>tr)caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}

        case PUSH_RETADDR:{int32_t o=CAML_READ_INT32(pc);pc+=4;caml_stack_push(Val_long(extra_args));caml_stack_push(env_v);caml_stack_push((value)(uintptr_t)(pc+o));NEXT();break;}
        case APPLY:{uint8_t n=NEXT_U8();value cl=accu;value se=env_v;env_v=cl;extra_args=(int)n-1;caml_stack_push(Val_long(extra_args));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t off=(size_t)Long_val(Field(cl,0));if(off<(size_t)(code_end-code_start))pc=code_start+off;NEXT();break;}
        case APPLY1:{value cl=accu,se=env_v;env_v=cl;extra_args=0;caml_stack_push(Val_long(0));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=(size_t)Long_val(Field(cl,0));if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case APPLY2:{value cl=accu,se=env_v;env_v=cl;extra_args=1;caml_stack_push(Val_long(1));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=(size_t)Long_val(Field(cl,0));if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case APPLY3:{value cl=accu,se=env_v;env_v=cl;extra_args=2;caml_stack_push(Val_long(2));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=(size_t)Long_val(Field(cl,0));if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case GRAB:{uint8_t r=NEXT_U8();if(extra_args<r){halted=1;return;}NEXT();break;}
        case RETURN:{uint8_t n=NEXT_U8();value sp=caml_stack_pop(),se=caml_stack_pop(),sx=caml_stack_pop();extra_args=(int)Long_val(sx);env_v=se;pc=(const uint8_t*)(uintptr_t)sp;while(n--)caml_stack_pop();NEXT();break;}
        case RESTART:NEXT();break;
        case APPTERM:case APPTERM1:case APPTERM2:case APPTERM3:{halted=1;return;}

        case CLOSURE:{int32_t co=CAML_READ_INT32(pc);pc+=4;mlsize_t nv=NEXT_U8();uint8_t ar=NEXT_U8();value cl=caml_alloc(2+nv,Closure_tag);Field(cl,0)=Val_long(co);Field(cl,1)=Val_long((intptr_t)ar);for(mlsize_t i=0;i<nv;i++)Field(cl,2+i)=caml_stack_pop();accu=cl;NEXT();break;}
        case CLOSUREREC:{halted=1;return;}

        case GETGLOBAL:{uint32_t s=NEXT_U32();accu=caml_global_get((mlsize_t)s);break;}
        case SETGLOBAL:{uint32_t s=NEXT_U32();caml_global_set((mlsize_t)s,accu);NEXT();break;}
        case GETGLOBALFIELD:{uint32_t s=NEXT_U32();uint8_t i=NEXT_U8();accu=Field(caml_global_get((mlsize_t)s),(mlsize_t)i);NEXT();break;}
        case PUSHGETGLOBAL:{uint32_t s=NEXT_U32();caml_stack_push(accu);accu=caml_global_get((mlsize_t)s);break;}
        case PUSHGETGLOBALFIELD:{uint32_t s=NEXT_U32();uint8_t i=NEXT_U8();value g=caml_global_get((mlsize_t)s);caml_stack_push(accu);accu=Field(g,(mlsize_t)i);NEXT();break;}
        case ATOM0:{uint32_t i=NEXT_U32();(void)i;NEXT();break;}

        case C_CALL1:{int i=(int)NEXT_U32();value a=caml_stack_pop();accu=p_dispatch(i,&a,1);NEXT();break;}
        case C_CALL2:{int i=(int)NEXT_U32();value a2=caml_stack_pop(),a1=caml_stack_pop();value args[2]={a1,a2};accu=p_dispatch(i,args,2);NEXT();break;}
        case C_CALL3:{int i=(int)NEXT_U32();value a3=caml_stack_pop(),a2=caml_stack_pop(),a1=caml_stack_pop();value args[3]={a1,a2,a3};accu=p_dispatch(i,args,3);NEXT();break;}
        case C_CALL4:case C_CALL5:case C_CALLN:{int i=(int)NEXT_U32();int n=NEXT_U8();value args[8];for(int j=n-1;j>=0;j--)args[j]=caml_stack_pop();accu=p_dispatch(i,args,n);NEXT();break;}
        case CHECK_SIGNALS:NEXT();break;

        case ENVACC1:accu=Field(env_v,2);NEXT();break;
        case ENVACC2:accu=Field(env_v,3);NEXT();break;
        case ENVACC3:accu=Field(env_v,4);NEXT();break;
        case ENVACC4:accu=Field(env_v,5);NEXT();break;
        case ENVACC:{uint8_t n=NEXT_U8();accu=Field(env_v,(mlsize_t)n+2);break;}
        case PUSHENVACC1:caml_stack_push(accu);accu=Field(env_v,2);NEXT();break;
        case PUSHENVACC2:caml_stack_push(accu);accu=Field(env_v,3);NEXT();break;
        case PUSHENVACC3:caml_stack_push(accu);accu=Field(env_v,4);NEXT();break;
        case PUSHENVACC4:caml_stack_push(accu);accu=Field(env_v,5);NEXT();break;
        case PUSHENVACC:{uint8_t n=NEXT_U8();caml_stack_push(accu);accu=Field(env_v,(mlsize_t)n+2);break;}
        case OFFSETCLOSUREM3:case OFFSETCLOSURE0:case OFFSETCLOSURE3:case OFFSETCLOSURE:
        case PUSHOFFSETCLOSUREM3:case PUSHOFFSETCLOSURE0:case PUSHOFFSETCLOSURE3:case PUSHOFFSETCLOSURE:{halted=1;return;}
        case MAKEFLOATBLOCK:{uint8_t t=NEXT_U8();uint16_t s=CAML_READ_UINT16(pc);pc+=2;(void)t;(void)s;NEXT();break;}
        case GETFLOATFIELD:case SETFLOATFIELD:{uint8_t f=NEXT_U8();(void)f;NEXT();break;}
        case PUSHCONST0:caml_stack_push(accu);accu=Val_long(0);NEXT();break;
        case PUSHCONST1:caml_stack_push(accu);accu=Val_long(1);NEXT();break;
        case PUSHCONST2:caml_stack_push(accu);accu=Val_long(2);NEXT();break;
        case PUSHCONST3:caml_stack_push(accu);accu=Val_long(3);NEXT();break;
        case ATOM:{uint8_t n=NEXT_U8();(void)n;NEXT();break;}
        case PUSHATOM0:caml_stack_push(accu);accu=Val_long(0);NEXT();break;
        case PUSHATOM:{uint8_t n=NEXT_U8();caml_stack_push(accu);(void)n;NEXT();break;}
        case VECTLENGTH:accu=Val_long(Wosize_hd(Hd_val(accu)));NEXT();break;
        case SWITCH:{uint32_t sz=NEXT_U32();uint32_t def=NEXT_U32();uint32_t idx=(uint32_t)Long_val(accu);if(idx<sz){int32_t o=CAML_READ_INT32(pc+idx*4);pc+=o;}else{pc=code_start+def;};break;}
        case BULTINT:{int32_t o=CAML_READ_INT32(pc);if((uintptr_t)Long_val(caml_stack_pop())<(uintptr_t)Long_val(accu))pc+=o;else pc+=4;break;}
        case BUGEINT:{int32_t o=CAML_READ_INT32(pc);if((uintptr_t)Long_val(caml_stack_pop())>=(uintptr_t)Long_val(accu))pc+=o;else pc+=4;break;}
        case GETBYTESCHAR:case SETBYTESCHAR:case GETSTRINGCHAR:NEXT();break;
        case GETFIELD0:accu=Field(accu,0);NEXT();break;
        case GETFIELD1:accu=Field(accu,1);NEXT();break;
        case GETFIELD2:accu=Field(accu,2);NEXT();break;
        case GETFIELD3:accu=Field(accu,3);NEXT();break;
        case SETFIELD0:Field(accu,0)=caml_stack_pop();NEXT();break;
        case SETFIELD1:Field(accu,1)=caml_stack_pop();NEXT();break;
        case SETFIELD2:Field(accu,2)=caml_stack_pop();NEXT();break;
        case SETFIELD3:Field(accu,3)=caml_stack_pop();NEXT();break;
        case GETMETHOD:case GETPUBMET:case GETDYNMET:{halted=1;return;}
        case EVENT:case BREAK:NEXT();break;

        case STOP:halted=1;return;
        default:halted=1;return;
        }
        if(--yield_counter<=0){yield_counter=YIELD_INTERVAL;return;}
        if(caml_stack_check_overflow()){halted=1;return;}
    }
}

value caml_get_acc(void){return accu;}
void caml_set_acc(value v){accu=v;}
value caml_get_env(void){return env_v;}
value* caml_get_trap_ptr(void){return trap;}
value caml_get_sp(mlsize_t s){return caml_stack_pointer()[s];}
value caml_get_global(mlsize_t s){return caml_global_get(s);}
void caml_set_global(mlsize_t s,value v){caml_global_set(s,v);}
int caml_vm_halted(void){return halted;}
size_t caml_get_instruction_count(void){return instruction_count;}
void caml_set_trace_hook(caml_trace_fn fn){trace_hook=fn;}

value caml_callback(value closure,value arg){
    value se=env_v,sa=accu;int sx=extra_args;
    caml_stack_push(arg);env_v=closure;extra_args=0;
    caml_stack_push(Val_long(0));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);
    size_t o=(size_t)Long_val(Field(closure,0));
    if(o<(size_t)(code_end-code_start))pc=code_start+o;
    caml_interpret();
    value r=accu;accu=sa;env_v=se;extra_args=sx;return r;
}
value caml_callback2(value c,value a1,value a2){caml_stack_push(a1);caml_stack_push(a2);return caml_callback(c,a2);}
void caml_process_events(void){}
