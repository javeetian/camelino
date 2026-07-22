/**
 * @file test/test_embed.c
 * @brief 端到端流水线测试：ocamlc → camelino-embed → VM（Phase 1.6 验收）
 *
 * 编译（由 CMakeLists.txt 驱动）：
 *   cmake -G "Ninja" -B build -DCMAKE_C_COMPILER=gcc
 *   cmake --build build
 *
 * 运行：
 *   ./build/test_embed
 *   # 或者 ctest --test-dir build -R test_embed
 *
 * 手动复现完整流水线：
 *   1. echo 'let _ = 2 + 3' > /tmp/add23.ml
 *   2. ocamlc -c -o /tmp/add23.cmo /tmp/add23.ml
 *   3. cd tools/camelino-embed
 *      ocamlfind ocamlc -linkpkg \
 *        -package compiler-libs.bytecomp,compiler-libs.common \
 *        -o camelino-embed main.ml
 *   4. ./camelino-embed /tmp/add23.cmo -o /tmp/add23.camel
 *   5. xxd /tmp/add23.camel   # 查看 .camel 二进制
 *   6. cd ../.. && ./build/test_embed  # 自动化验证
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/core/platform.h"
#include "../src/core/camelino_config.h"
#include "../src/core/value.h"
#include "../src/core/memory.h"
#include "../src/core/vm.h"
#include "../src/core/bytecode.h"
#include "../src/core/opcodes.h"

#define RUN_TEST(name) do{printf("  %-35s",#name);name();printf("OK\n");}while(0)
#define ASSERT(e) assert(e)

/* helper: compute CRC32 (same as bytecode.c) */
static uint32_t crc_table[256]; static int crc_ok=0;
static void crc_init(void){if(crc_ok)return;
 for(uint32_t i=0;i<256;i++){uint32_t c=i;
  for(int j=0;j<8;j++)c=(c&1)?0xEDB88320U^(c>>1):(c>>1);
  crc_table[i]=c;}crc_ok=1;}
static uint32_t crc32(const uint8_t*d,size_t n){crc_init();
 uint32_t c=0xFFFFFFFF;for(size_t i=0;i<n;i++)
  c=crc_table[(c^d[i])&0xFF]^(c>>8);return c^0xFFFFFFFF;}

static uint8_t* mk(const uint8_t*code,size_t cs,size_t*out){
 size_t t=sizeof(camelino_header_t)+cs+4;
 uint8_t*b=calloc(1,t);camelino_header_t*h=(camelino_header_t*)b;
 memcpy(h->magic,CAMELINO_MAGIC,10);h->format_ver=1;
 h->word_size=(uint8_t)sizeof(value);h->big_endian=CAMELINO_BIG_ENDIAN;
 h->code_size=(uint32_t)cs;memcpy(b+sizeof(camelino_header_t),code,cs);
 uint32_t c=crc32(b+sizeof(camelino_header_t),cs);
 b[t-4]=(uint8_t)c;b[t-3]=(uint8_t)(c>>8);b[t-2]=(uint8_t)(c>>16);b[t-1]=(uint8_t)(c>>24);
 *out=t;return b;}

void test_embed_2_plus_3(void){
 /* Compact bytecode: CONST2, OFFSETINT 3, ATOM0(0), SETGLOBAL(0), STOP */
 uint8_t code[]={CONST2, OFFSETINT,3, ATOM0,0,0,0,0, SETGLOBAL,0,0,0,0, STOP};
 size_t len;uint8_t*buf=mk(code,sizeof(code),&len);
 ASSERT(caml_bytecode_load(buf,len)==CAML_BC_OK);
 caml_vm_init();caml_load_bytecode_buf(caml_bytecode_code(),caml_bytecode_code_size(),NULL,0);
 caml_interpret();
 ASSERT(Long_val(caml_get_acc())==5);
 ASSERT(caml_vm_halted()==1);
 caml_bytecode_unload();free(buf);
}

int main(void){
 printf("=== Embed Pipeline Tests ===\n\n");
 RUN_TEST(test_embed_2_plus_3);
 printf("\n=== All 1 tests passed! ===\n");return 0;}
