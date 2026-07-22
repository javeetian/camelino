#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
int main(void){
 FILE* f = fopen("_t.camel", "rb");
 if(!f){fprintf(stderr,"FAIL: no file\n");return 1;}
 fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
 fprintf(stderr,"len=%ld\n",len);
 uint8_t* buf=malloc(len);fread(buf,1,len,f);fclose(f);
 fprintf(stderr,"buf[0..9]=%c%c%c%c%c%c%c%c%c%c\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);
 int cs=(int)buf[20]|((int)buf[21]<<8)|((int)buf[22]<<16)|((int)buf[23]<<24);
 fprintf(stderr,"code_size=%d\n",cs);
 for(int i=0;i<cs;i++)fprintf(stderr,"%02x ",buf[32+i]);
 fprintf(stderr,"\n");
 free(buf); return 0;
}
