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

/* Get code offset from closure (handles infix closures from CLOSUREREC).
   Normal closure: Field 0 = Val_long(offset).
   Infix closure: Field 0 = Make_header(Infix_tag), Field 1 = Val_long(offset). */
static size_t closure_code_offset(value cl) {
    if ((Field(cl, 0) & 0xFF) == Infix_tag)
        return (size_t)Long_val(Field(cl, 2));
    return (size_t)Long_val(Field(cl, 0));
}

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
void caml_load_bytecode_buf(const uint8_t* c,size_t cs,const uint8_t*d,size_t ds){
    code_start=c;code_end=c+cs;pc=code_start;halted=0;
    if(d&&ds>0){
        size_t nwords=ds/4;
        if(nwords>MAX_GLOBALS)nwords=MAX_GLOBALS;
        for(size_t i=0;i<nwords;i++){
            value v=(value)CAML_READ_UINT32(d+i*4);
            caml_global_set((mlsize_t)i,v);
        }
    }
    /* Build OCaml-style callback frame (matching runtime/callback.c caml_callbackN_exn).
       For narg=0 (module init): sp[0]=return_pc, sp[1]=Val_unit, sp[2]=Val_long(0), sp[3]=closure.
       The closure is a minimal 2-field block pointing to bytecode entry at offset 0.
       APPLY (simulated inline) pushes return frame [extra_args, saved_env, return_pc]
       and jumps to the closure code. */
    value cl = caml_alloc(2, Closure_tag);
    Field(cl, 0) = Val_long(0);     /* code offset: entry point */
    Field(cl, 1) = Val_long(0);     /* closinfo: arity=0 */
    caml_stack_push(cl);                 /* sp[3] after all pushes */
    caml_stack_push(Val_long(0));        /* sp[2] = extra_args = 0 */
    caml_stack_push(Val_unit);           /* sp[1] = env */
    caml_stack_push((value)(uintptr_t)code_end); /* sp[0] = return_addr */

    /* Simulate ACC(narg+3) → read closure from sp[3]; APPLY(narg=0) → push frame + jump */
    accu = cl;
    env_v = cl;
    extra_args = -1; /* narg-1 = -1 for 0-arg apply */
    caml_stack_push(Val_long(extra_args));
    caml_stack_push(Val_unit);
    caml_stack_push((value)(uintptr_t)code_end);
    pc = code_start; /* entry offset 0 */
    halted = 0;
}

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

        case BEQ:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())==Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BNEQ:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())!=Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BLTINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())<Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BLEINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())<=Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BGTINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())>Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BGEINT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(caml_stack_pop())>=Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case ULTINT:accu=Val_bool((uintptr_t)Long_val(caml_stack_pop())<(uintptr_t)Long_val(accu));NEXT();break;
        case UGEINT:accu=Val_bool((uintptr_t)Long_val(caml_stack_pop())>=(uintptr_t)Long_val(accu));NEXT();break;
        case BOOLNOT:accu=Val_bool(Long_val(accu)==0);NEXT();break;

        case OFFSETREF:{int8_t n=NEXT_S8();Field(accu,0)=Val_long(Long_val(Field(accu,0))+n);NEXT();break;}
        case ISINT:accu=Val_bool(Is_long(accu));NEXT();break;

        case BRANCH:{int32_t o=CAML_READ_INT32(pc);const uint8_t* target=code_start+o;if(target<code_start||target>=code_end){halted=1;return;}pc=target;break;}
        case BRANCHIF:{int32_t o=CAML_READ_INT32(pc);if(Long_val(accu)!=0)pc=code_start+o;else pc+=4;break;}
        case BRANCHIFNOT:{int32_t o=CAML_READ_INT32(pc);if(Long_val(accu)==0)pc=code_start+o;else pc+=4;break;}

        case PUSHTRAP:{int32_t o=CAML_READ_INT32(pc);pc+=4;caml_stack_push(accu);caml_stack_push(Val_long(extra_args));caml_stack_push(env_v);caml_stack_push((value)(uintptr_t)(pc+o));trap=caml_stack_pointer();NEXT();break;}
        case POPTRAP:{value*sp=caml_stack_pointer();accu=sp[3];extra_args=(int)Long_val(sp[2]);env_v=sp[1];caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}
        case RAISE:case RAISE_NOTRACE:{if(!trap){halted=1;return;}value*tr=trap;pc=(const uint8_t*)(uintptr_t)tr[0];env_v=tr[1];extra_args=(int)Long_val(tr[2]);while(caml_stack_pointer()>tr)caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}
        case RERAISE:{if(!trap){halted=1;return;}value*tr=trap;pc=(const uint8_t*)(uintptr_t)tr[0];env_v=tr[1];extra_args=(int)Long_val(tr[2]);while(caml_stack_pointer()>tr)caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();caml_stack_pop();trap=NULL;NEXT();break;}

        case PUSH_RETADDR:{int32_t o=CAML_READ_INT32(pc);pc+=4;caml_stack_push(Val_long(extra_args));caml_stack_push(env_v);caml_stack_push((value)(uintptr_t)(pc+o));NEXT();break;}
        case APPLY:{uint8_t n=NEXT_U8();value cl=accu;value se=env_v;env_v=cl;extra_args=(int)n-1;caml_stack_push(Val_long(extra_args));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t off=closure_code_offset(cl);if(off<(size_t)(code_end-code_start))pc=code_start+off;NEXT();break;}
        case APPLY1:{value cl=accu,se=env_v;env_v=cl;extra_args=0;caml_stack_push(Val_long(0));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=closure_code_offset(cl);if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case APPLY2:{value cl=accu,se=env_v;env_v=cl;extra_args=1;caml_stack_push(Val_long(1));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=closure_code_offset(cl);if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case APPLY3:{value cl=accu,se=env_v;env_v=cl;extra_args=2;caml_stack_push(Val_long(2));caml_stack_push(se);caml_stack_push((value)(uintptr_t)pc);size_t o=closure_code_offset(cl);if(o<(size_t)(code_end-code_start))pc=code_start+o;NEXT();break;}
        case GRAB:{
            uint8_t r=NEXT_U8();
            if(extra_args>=r){extra_args-=r;NEXT();}
            else if(extra_args+1>=r){extra_args=0;NEXT();}
            else{halted=1;return;}
            break;
        }
        case RETURN:{
            uint8_t n=NEXT_U8();
            value sp_ret=caml_stack_pop(),se=caml_stack_pop(),sx=caml_stack_pop();
            extra_args=(int)Long_val(sx);env_v=se;pc=(const uint8_t*)(uintptr_t)sp_ret;
            while(n--)caml_stack_pop();
            NEXT();break;
        }
        case RESTART:{
            /* Reference: OCaml 4.14 interp.c Instruct(RESTART)
               Reorganize stack from partial application: env holds saved args */
            mlsize_t num_args = Wosize_hd(Hd_val(env_v)) - 3;
            for(mlsize_t i=0;i<num_args;i++) caml_stack_push(Field(env_v,3+i));
            env_v=Field(env_v,2);
            extra_args+=(int)num_args;
            NEXT();break;
        }
        case APPTERM:case APPTERM1:case APPTERM2:case APPTERM3:{
            uint8_t nargs=(op==APPTERM)?NEXT_U8():(uint8_t)(op-APPTERM1+1);
            uint8_t slotsize=NEXT_U8();
            value cl=accu;
            mlsize_t n=(mlsize_t)nargs+3+(mlsize_t)slotsize;
            while(n--)caml_stack_pop();
            env_v=cl;
            size_t off=closure_code_offset(cl);
            if(off<(size_t)(code_end-code_start))pc=code_start+off;
            else{halted=1;return;}
            break;
        }

        case CLOSURE:{
            mlsize_t nv=NEXT_U8();
            int32_t co=CAML_READ_INT32(pc);pc+=4;
            value cl=caml_alloc(2+nv,Closure_tag);
            Field(cl,0)=Val_long(co);
            Field(cl,1)=Val_long((intptr_t)(nv<<1));
            for(mlsize_t i=nv;i>0;i--)Field(cl,1+i)=caml_stack_pop();
            accu=cl;NEXT();break;
        }
        case CLOSUREREC:{
            mlsize_t nfuncs=NEXT_U8(),nvars=NEXT_U8();
            /* envofs = nfuncs*3 - 1: each function uses 3 words minus 1 overlap.
               +1 extra field for OFFSETCLOSURE access (e.g. f0 accesses f1 at envofs=5) */
            mlsize_t envofs=nfuncs*3-1;
            mlsize_t wsz=envofs+nvars+1;  /* +1: OFFSETCLOSURE reads Field(env, 2+n) up to envofs */
            value cl=caml_alloc(wsz,Closure_tag);
            value* p=&Field(cl,0);
            /* build infix table: first function code pointer + closinfo, rest infix headers */
            for(mlsize_t i=0;i<nfuncs;i++){
                int8_t off = (int8_t)NEXT_U8();
                intptr_t rel = (intptr_t)((pc - 1 - code_start) + off);
                if(i==0){
                    *p++ = Val_long(rel);
                    *p++ = /* self-pointer for OFFSETCLOSURE access */ (value)(void*)p;
                } else {
                    envofs -= 3;
                    *p++ = Make_header((header_t)(i*3),Infix_tag,0);
                    caml_stack_push((value)(void*)p);  /* push infix pointer (OCaml: *--sp = p) */
                    *p++ = Val_long(rel);
                    *p++ = /* self-pointer for OFFSETCLOSURE access */ (value)(void*)p;
                }
            }
            /* copy free variables from stack */
            for(mlsize_t i=nvars;i>0;i--)Field(cl,envofs+i-1)=caml_stack_pop();
            /* store f1's infix pointer at envofs for f0's OFFSETCLOSURE access */
            if(nfuncs>=2) Field(cl, envofs) = Field(cl, 2);
            /* all nfuncs offsets consumed by the loop above */
            caml_stack_push(cl); /* push closure to stack (OCaml: *--sp = accu) */
            accu=cl;NEXT();break;
        }

        case GETGLOBAL:{uint32_t s=NEXT_U32();accu=caml_global_get((mlsize_t)s);break;}
        case SETGLOBAL:{uint32_t s=NEXT_U32();caml_global_set((mlsize_t)s,accu);NEXT();break;}
        case GETGLOBALFIELD:{uint32_t s=NEXT_U32();uint8_t i=NEXT_U8();accu=Field(caml_global_get((mlsize_t)s),(mlsize_t)i);NEXT();break;}
        case PUSHGETGLOBAL:{uint32_t s=NEXT_U32();caml_stack_push(accu);accu=caml_global_get((mlsize_t)s);break;}
        case PUSHGETGLOBALFIELD:{uint32_t s=NEXT_U32();uint8_t i=NEXT_U8();value g=caml_global_get((mlsize_t)s);caml_stack_push(accu);accu=Field(g,(mlsize_t)i);NEXT();break;}
        case ATOM0:NEXT();break; /* NOP: reads globals[0] when atom table loaded */

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
        case OFFSETCLOSUREM3:accu=Field(env_v,2-3);NEXT();break;
        case OFFSETCLOSURE0:accu=Field(env_v,2);NEXT();break;
        case OFFSETCLOSURE3:accu=Field(env_v,2+3);NEXT();break;
        case OFFSETCLOSURE:{uint8_t n=NEXT_U8();accu=Field(env_v,(mlsize_t)n+2);break;}
        case PUSHOFFSETCLOSUREM3:caml_stack_push(accu);accu=Field(env_v,2-3);NEXT();break;
        case PUSHOFFSETCLOSURE0:caml_stack_push(accu);accu=Field(env_v,2);NEXT();break;
        case PUSHOFFSETCLOSURE3:caml_stack_push(accu);accu=Field(env_v,2+3);NEXT();break;
        case PUSHOFFSETCLOSURE:{uint8_t n=NEXT_U8();caml_stack_push(accu);accu=Field(env_v,2+(mlsize_t)n);break;}
        case MAKEFLOATBLOCK:{
            mlsize_t sz = NEXT_U8();  /* number of doubles */
            mlsize_t wosize = sz * (sizeof(double) / sizeof(value));
            value b = caml_alloc(wosize, Double_array_tag);
            double* dp = (double*)(void*)&Field(b, 0);
            dp[sz-1] = *(double*)(void*)&accu;
            for(mlsize_t i=1;i<sz;i++){
                value v = caml_stack_pop();
                double d; memcpy(&d, &Field(v,0), sizeof(double));
                dp[sz-1-i] = d;
            }
            accu = b; NEXT();break;
        }
        case GETFLOATFIELD:{
            uint8_t idx=NEXT_U8();
            mlsize_t off=(mlsize_t)idx*(sizeof(double)/sizeof(value));
            double d; memcpy(&d,(char*)accu+off*sizeof(value),sizeof(double));
            value box=caml_alloc(sizeof(double)/sizeof(value),Double_tag);
            memcpy(&Field(box,0),&d,sizeof(double));
            accu=box;NEXT();break;
        }
        case SETFLOATFIELD:{
            uint8_t idx=NEXT_U8();
            value src=caml_stack_pop();
            double d; memcpy(&d,&Field(src,0),sizeof(double));
            mlsize_t off=(mlsize_t)idx*(sizeof(double)/sizeof(value));
            memcpy((char*)accu+off*sizeof(value),&d,sizeof(double));
            accu=Val_unit;NEXT();break;
        }
        case PUSHCONST0:caml_stack_push(accu);accu=Val_long(0);NEXT();break;
        case PUSHCONST1:caml_stack_push(accu);accu=Val_long(1);NEXT();break;
        case PUSHCONST2:caml_stack_push(accu);accu=Val_long(2);NEXT();break;
        case PUSHCONST3:caml_stack_push(accu);accu=Val_long(3);NEXT();break;
        case ATOM:{uint32_t idx=NEXT_U32();accu=caml_global_get((mlsize_t)idx);NEXT();break;}
        case PUSHATOM0:caml_stack_push(accu);NEXT();break; /* PUSH + ATOM0 */
        case PUSHATOM:{uint32_t idx=NEXT_U32();caml_stack_push(accu);accu=caml_global_get((mlsize_t)idx);NEXT();break;}
        case MAKEBLOCK:case MAKEBLOCK1:case MAKEBLOCK2:case MAKEBLOCK3:{
            uint8_t tag=NEXT_U8();
            uint16_t sz=CAML_READ_UINT16(pc);pc+=2;
            int pre=(op==MAKEBLOCK)?0:(op==MAKEBLOCK1)?1:(op==MAKEBLOCK2)?2:3;
            value b=caml_alloc((mlsize_t)(pre+sz),(tag_t)tag);
            value* sp=caml_stack_pointer(); /* use OCaml convention: read from stack without popping */
            for(mlsize_t i=0;i<(mlsize_t)sz;i++)
                Field(b,(mlsize_t)(pre+sz-1-i))=sp[i];
            if(pre)Field(b,0)=accu;
            /* pop sz items from stack */
            for(mlsize_t i=0;i<(mlsize_t)sz;i++)caml_stack_pop();
            accu=b;NEXT();break;
        }
        case VECTLENGTH:accu=Val_long(Wosize_hd(Hd_val(accu)));NEXT();break;
        case GETVECTITEM:{value idx=caml_stack_pop();accu=Field(accu,(mlsize_t)Long_val(idx));NEXT();break;}
        case SETVECTITEM:{value v=caml_stack_pop(),idx=caml_stack_pop();Field(v,(mlsize_t)Long_val(idx))=accu;accu=Val_unit;NEXT();break;}
        case GETFIELD:{uint8_t n=NEXT_U8();accu=Field(accu,(mlsize_t)n);NEXT();break;}
        case SETFIELD:{uint8_t n=NEXT_U8();Field(accu,(mlsize_t)n)=caml_stack_pop();NEXT();break;}
        case SWITCH:{uint32_t sz=NEXT_U32();uint32_t def=NEXT_U32();uint32_t idx=(uint32_t)Long_val(accu);if(idx<sz){int32_t o=CAML_READ_INT32(pc+idx*4);/*FIXED*/ pc=code_start+o;}else{pc=code_start+def;};break;}
        case BULTINT:{int32_t o=CAML_READ_INT32(pc);if((uintptr_t)Long_val(caml_stack_pop())<(uintptr_t)Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case BUGEINT:{int32_t o=CAML_READ_INT32(pc);if((uintptr_t)Long_val(caml_stack_pop())>=(uintptr_t)Long_val(accu))pc=code_start+o;else pc+=4;break;}
        case GETBYTESCHAR:case GETSTRINGCHAR:{
            value idx=caml_stack_pop();
            accu=Val_long((intptr_t)((unsigned char*)(void*)accu)[Long_val(idx)]);
            NEXT();break;
        }
        case SETBYTESCHAR:{
            value v=caml_stack_pop(),idx=caml_stack_pop();
            ((unsigned char*)(void*)accu)[Long_val(idx)]=(unsigned char)(Long_val(v)&0xFF);
            accu=Val_unit;NEXT();break;
        }
        case GETFIELD0:accu=Field(accu,0);NEXT();break;
        case GETFIELD1:accu=Field(accu,1);NEXT();break;
        case GETFIELD2:accu=Field(accu,2);NEXT();break;
        case GETFIELD3:accu=Field(accu,3);NEXT();break;
        case SETFIELD0:Field(accu,0)=caml_stack_pop();NEXT();break;
        case SETFIELD1:Field(accu,1)=caml_stack_pop();NEXT();break;
        case SETFIELD2:Field(accu,2)=caml_stack_pop();NEXT();break;
        case SETFIELD3:Field(accu,3)=caml_stack_pop();NEXT();break;
        case GETMETHOD:{
            /* OCaml ref: interp.c ln 1082-1086  Lookup(obj,lab) = Field(Field(obj,0), Int_val(lab)) */
            accu = Field(Field(caml_stack_pointer()[0], 0), (mlsize_t)Long_val(accu));
            NEXT();break;
        }
        case GETPUBMET:{
            /* camelino-embed emits INT32 (4 bytes): tag_byte + cache_byte + 2 unused */
            uint8_t tag = NEXT_U8(); uint8_t cache = NEXT_U8(); pc += 2; (void)tag; (void)cache;
        }
        /* FALLTHROUGH to GETDYNMET */
        case GETDYNMET:{
            /* OCaml ref: interp.c ln 1132-1143. Binary search method table for accu(tag).
               Method table: Field(obj,0)=meths, Field(meths,0)=size(tagged), Field(meths,1)=cache
               Fields 2.. = {closure, tag} pairs ordered by tag ascending. */
            value obj = caml_stack_pointer()[0];
            value meths = Field(obj, 0);
            int li = 3, hi = (int)Field(meths, 0), mi;
            /* If size is tagged (bit0=1), untag it */
            if(hi & 1) hi = (int)Long_val((value)(intptr_t)hi);
            while(li < hi){
                mi = ((li + hi) >> 1) | 1;  /* ensure odd (tag position) */
                if(accu < Field(meths, (mlsize_t)mi)) hi = mi - 2;
                else li = mi;
            }
            accu = Field(meths, (mlsize_t)(li - 1));  /* closure at even position */
            pc += 4;  /* camelino-embed emits INT32 (4 bytes) for GETDYNMET */
            NEXT();break;
        }
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
