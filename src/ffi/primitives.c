/* Phase 2 内部 C primitive 桩 */

#include "platform.h"
#include "value.h"
#include "vm.h"
#include <stdio.h>

/* failwith — 暂用 fatal error */
value caml_failwith(value msg) { (void)msg; fprintf(stderr,"failwith stub\n"); return Val_unit; }
value caml_invalid_argument(value msg) { (void)msg; fprintf(stderr,"invalid_arg stub\n"); return Val_unit; }
value caml_array_bound_error(void) { fprintf(stderr,"array bound stub\n"); return Val_unit; }
value caml_raise_with_arg(value exn, value arg) { (void)exn;(void)arg; return Val_unit; }
value caml_raise_with_string(value exn, value s) { (void)exn;(void)s; return Val_unit; }
value caml_install_signal_handler(value a,value b) { (void)a;(void)b; return Val_unit; }
value caml_sys_exit(value code) { (void)code; return Val_unit; }
