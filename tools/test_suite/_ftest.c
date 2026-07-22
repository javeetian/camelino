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
