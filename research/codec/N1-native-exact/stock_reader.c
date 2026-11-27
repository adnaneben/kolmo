/* Independent minimal reader: unmodified OpenZL public decoder, no parser/custom codec. */
#include <openzl/zl_decompress.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END))return 2;long sn=ftell(f);
    if(sn<0||(uint64_t)sn>(258u<<20))return 2;rewind(f);
    void *src=malloc(sn? (size_t)sn:1);if(!src)return 2;
    if(fread(src,1,(size_t)sn,f)!=(size_t)sn){free(src);fclose(f);return 2;}fclose(f);
    ZL_Report n=ZL_getDecompressedSize(src,(size_t)sn),c=ZL_getCompressedSize(src,(size_t)sn);
    if(ZL_isError(n)||ZL_isError(c)||ZL_validResult(n)>(256u<<20)||ZL_validResult(c)!=(size_t)sn){free(src);return 2;}
    size_t size=ZL_validResult(n);void *dst=malloc(size?size:1);if(!dst){free(src);return 2;}
    ZL_Report r=ZL_decompress(dst,size,src,(size_t)sn);free(src);
    if(ZL_isError(r)||ZL_validResult(r)!=size){free(dst);return 2;}
    f=fopen(argv[2],"wbx");if(!f){free(dst);return 2;}
    int ok=fwrite(dst,1,size,f)==size;free(dst);if(fclose(f))ok=0;return ok?0:2;
}
