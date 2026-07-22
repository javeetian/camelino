#include "platform.h"
#include "camelino_config.h"
#include "value.h"
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
#include <stdio.h>
#include <stdlib.h>
int main(void){
 FILE* f = fopen("_t.camel", "rb"); fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
 uint8_t* buf = malloc(len); fread(buf,1,len,f); fclose(f);
 fprintf(stderr, "file_len=%ld\n", len);
 int rc = caml_bytecode_load(buf, len);
 fprintf(stderr, "load_rc=%d code_size=%zu\n", rc, caml_bytecode_code_size());
 const uint8_t* code = caml_bytecode_code();
 fprintf(stderr, "code[0..%zu]: ", caml_bytecode_code_size());
 for(size_t i=0;i<caml_bytecode_code_size();i++) fprintf(stderr,"%02x ",code[i]);
 fprintf(stderr,"\n");
 caml_vm_init();
 caml_load_bytecode_buf(code, caml_bytecode_code_size(), NULL, 0);
 caml_interpret();
 fprintf(stderr, "acc=%ld\n", (long)Long_val(caml_get_acc()));
 caml_bytecode_unload(); free(buf); return 0;
}
