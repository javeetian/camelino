#include "../../src/core/platform.h"
#include "../../src/core/camelino_config.h"
#include "../../src/core/bytecode.h"
#include <stdio.h>
#include <stdlib.h>
int main(void){
 FILE* f = fopen("_t.camel", "rb"); fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
 uint8_t* buf = malloc(len); fread(buf,1,len,f); fclose(f);
 int rc = caml_bytecode_load(buf, len);
 fprintf(stderr, "load_rc=%d cs=%zu code[0]=%02x\n", rc, caml_bytecode_code_size(), caml_bytecode_code()?caml_bytecode_code()[0]:0);
 free(buf); return 0;
}
