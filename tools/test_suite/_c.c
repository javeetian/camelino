#include <stdio.h>
#include "platform.h"
#include "camelino_config.h"
#include "value.h"
#include "vm.h"
#include "memory.h"
int main(void){uint8_t b[]={0x65,0x7f,0x03,0x3a,0x39,0x00,0x00,0x00,0x8f};caml_vm_init();caml_load_bytecode_buf(b,sizeof(b),NULL,0);caml_interpret();printf("%ld\n",(long)Long_val(caml_get_acc()));return 0;}
