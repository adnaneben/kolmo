/* Standalone reader of pinned leaf formats; framing/numeric inverse is separate Python. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
struct SBuf{unsigned char*p;size_t n;};
extern struct SBuf s_decompress(int,const unsigned char*,size_t,size_t);
int main(int ac,char**av){
 if(ac!=5)return 2;char*end;unsigned long long n=strtoull(av[3],&end,10);if(*end||n>(64u<<20))return 2;
 FILE*f=fopen(av[2],"rb");if(!f)return 2;if(fseek(f,0,SEEK_END))return 2;long z=ftell(f);if(z<0||z>(64<<20))return 2;rewind(f);
 unsigned char*p=malloc(z?z:1);if(!p||fread(p,1,z,f)!=(size_t)z)return 2;fclose(f);
 struct SBuf b=s_decompress(av[1][0],p,z,n);free(p);if(!b.p)return 3;
 f=fopen(av[4],"wb");if(!f){free(b.p);return 2;}int bad=fwrite(b.p,1,b.n,f)!=b.n;bad|=fclose(f)!=0;free(b.p);return bad?2:0;
}
