#include "frontier_fastq.h"
#include "frontier_entropy.h"
#include "frontier_runs.h"
#include <stdlib.h>
#include <string.h>

enum { META, TEXT, BASES, EXCEPTIONS, QUALITY, STREAMS, HEADER=16+9*STREAMS, MAX_READ=16384, MAX_HEAD=4096 };
typedef struct { const uint8_t *data; size_t len; } Slice;
typedef struct { Slice name,seq,plus,quality; unsigned ending; const uint8_t *after; } Read;
typedef struct { uint8_t *start,*at,*end; } Writer;
typedef struct { const uint8_t *at,*end; uint8_t *owned; } Reader;

static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t v) { for(unsigned j=0;j<4;++j) p[j]=(uint8_t)(v>>(8*j)); }
static int line(const uint8_t **p,const uint8_t *end,Slice *s,unsigned *ending,size_t maximum) {
    size_t n=(size_t)(end-*p);
    if(n>maximum+2) n=maximum+2;
    const uint8_t *lf=memchr(*p,'\n',n);
    if(!lf) return 0;
    *ending=1+(lf>*p && lf[-1]=='\r');
    s->data=*p;s->len=(size_t)(lf-*p)-(*ending-1);*p=lf+1;
    return s->len<=maximum;
}
static int record(const uint8_t *p,const uint8_t *end,Read *r) {
    if(p==end || *p!='@') return 0;
    unsigned a,b,c,d;
    if(!line(&p,end,&r->name,&a,MAX_HEAD) || !line(&p,end,&r->seq,&b,MAX_READ) ||
       !line(&p,end,&r->plus,&c,MAX_HEAD) || !line(&p,end,&r->quality,&d,MAX_READ)) return 0;
    if(a!=b || a!=c || a!=d || !r->seq.len || r->quality.len!=r->seq.len ||
       !r->plus.len || r->plus.data[0]!='+') return 0;
    r->ending=a;r->after=p;return 1;
}
static const uint8_t *start_of_records(const uint8_t *src,size_t n) {
    const uint8_t *end=src+n,*limit=src+(n<32768?n:32768),*p=src;
    while(p<limit) {
        const uint8_t *at=memchr(p,'@',(size_t)(limit-p));
        if(!at) break;
        Read first,second;
        if((at==src || at[-1]=='\n') && record(at,end,&first) && record(first.after,end,&second)) return at;
        p=at+1;
    }
    return NULL;
}
int frontier_fastq_probe(const uint8_t *src,size_t n) {
    if(n<1024 || (src[0]!='@' && !memchr(src,'@',n<512?n:512))) return 0;
    return start_of_records(src,n)!=NULL;
}
static int byte(Writer *w,unsigned v) {
    if(w->at==w->end) return 0;
    *w->at++=(uint8_t)v;return 1;
}
static int append(Writer *w, const uint8_t *p,size_t n) {
    if(n>(size_t)(w->end-w->at)) return 0;
    if(n) memcpy(w->at,p,n);
    w->at+=n;return 1;
}
static int var(Writer *w,size_t x) {
    while(x>=128) { if(!byte(w,128|(x&127))) return 0; x>>=7; }
    return byte(w,(unsigned)x);
}
static int read_var(Reader *r,size_t *value) {
    size_t x=0;
    for(unsigned shift=0;shift<32;shift+=7) {
        if(r->at==r->end) return 0;
        unsigned c=*r->at++;
        if(shift==28 && (c&127)>15) return 0;
        x|=(size_t)(c&127)<<shift;
        if(!(c&128)) {*value=x;return 1;}
    }
    return 0;
}
static int digit(uint8_t c) { return c>='0' && c<='9'; }

/* Increment a decimal token in its original width, including leading zeros.
 * The all-nines case grows by one digit. */
static size_t increment(Slice before,size_t off,uint8_t *dst,size_t cap) {
    if(off>=before.len || !digit(before.data[off]) || (off && digit(before.data[off-1]))) return 0;
    size_t end=off;
    while(end<before.len && digit(before.data[end])) ++end;
    size_t stop=end;
    while(stop>off && before.data[stop-1]=='9') --stop;
    size_t grow=stop==off;
    if(before.len+grow>cap) return 0;
    if(grow) {
        memcpy(dst,before.data,off);dst[off]='1';
        memset(dst+off+1,'0',end-off);
        memcpy(dst+end+1,before.data+end,before.len-end);
    } else {
        memcpy(dst,before.data,before.len);++dst[stop-1];
        memset(dst+stop,'0',end-stop);
    }
    return before.len+grow;
}

static int header(Writer *meta,Writer *text,Slice a,Slice b,size_t *last) {
    size_t prefix=0,suffix=0,common=a.len<b.len?a.len:b.len;
    while(prefix<common && a.data[prefix]==b.data[prefix]) ++prefix;
    if(prefix==a.len && prefix==b.len) return byte(meta,1);
    if(prefix<a.len && digit(a.data[prefix])) {
        size_t start=prefix;
        while(start && digit(a.data[start-1])) --start;
        uint8_t next[MAX_HEAD+1];
        size_t nn=increment(a,start,next,sizeof(next));
        if(nn==b.len && !memcmp(next,b.data,nn)) {
            if(start==*last) return byte(meta,2);
            *last=start;return byte(meta,3) && var(meta,start);
        }
    }
    while(suffix<common-prefix && a.data[a.len-1-suffix]==b.data[b.len-1-suffix]) ++suffix;
    size_t literal=b.len-prefix-suffix;
    return byte(meta,0) && var(meta,prefix) && var(meta,suffix) && var(meta,literal)
           && append(text,b.data+prefix,literal);
}
static const uint8_t alphabet[4]={'A','C','T','G'};
static int encode_bases(Writer *packed, Writer *exc, Slice seq,size_t base,size_t *last) {
    unsigned bits=0;
    for(size_t i=0;i<seq.len;++i) {
        unsigned c=seq.data[i],v=(c>>1)&3;
        bits|=v<<(2*(i%4));
        if(c!=alphabet[v]) {
            if(!var(exc,base+i-*last) || !byte(exc,c)) return 0;
            *last=base+i+1;
        }
        if(i%4==3 || i+1==seq.len) { if(!byte(packed,bits)) return 0; bits=0; }
    }
    return 1;
}

size_t frontier_fastq_encode(const uint8_t *src,size_t n,uint8_t *dst,size_t cap) {
    if(n<1024 || n>(32u<<20) || cap<=HEADER) return 0;
    const uint8_t *first=start_of_records(src,n),*end=src+n;
    if(!first) return 0;
    size_t rows=0,sites=0,text=0,packed=0;
    Read r;
    const uint8_t *p=first;
    while(record(p,end,&r)) {
        ++rows;sites+=r.seq.len;text+=r.name.len+r.plus.len;packed+=(r.seq.len+3)/4;p=r.after;
    }
    size_t prefix=(size_t)(first-src),suffix=(size_t)(end-p);
    if(rows<4 || sites<n/8 || prefix+suffix>cap-HEADER || suffix>32768) return 0;
    size_t sizes[STREAMS]={rows*24+32,text,packed,n/8,sites};
    size_t mem=0;
    for(unsigned i=0;i<STREAMS;++i) mem+=sizes[i];
    uint8_t *storage=malloc(mem);
    if(!storage) return 0;
    Writer s[STREAMS];
    uint8_t *at=storage;
    for(unsigned i=0;i<STREAMS;++i) { s[i]=(Writer){at,at,at+sizes[i]};at+=sizes[i]; }
    size_t result=0,last_digit=SIZE_MAX,last_site=0,site=0,previous_len=0;
    Slice prev={NULL,0};p=first;
    while(record(p,end,&r)) {
        unsigned flags=(r.ending==2) | ((r.seq.len==previous_len)<<1);
        unsigned plus=r.plus.len==1?0:r.plus.len==r.name.len && !memcmp(r.plus.data+1,r.name.data+1,r.name.len-1)?1:2;
        flags|=plus<<2;
        if(!byte(&s[META],flags) || (!(flags&2) && !var(&s[META],r.seq.len)) ||
           !header(&s[META],&s[TEXT],prev,r.name,&last_digit)) goto done;
        if(plus==2 && (!var(&s[META],r.plus.len-1) || !append(&s[TEXT],r.plus.data+1,r.plus.len-1))) goto done;
        if(!encode_bases(&s[BASES],&s[EXCEPTIONS],r.seq,site,&last_site) ||
           !append(&s[QUALITY],r.quality.data,r.quality.len)) goto done;
        previous_len=r.seq.len;prev=r.name;site+=r.seq.len;p=r.after;
    }
    put32(dst,(uint32_t)prefix);put32(dst+4,(uint32_t)suffix);put32(dst+8,(uint32_t)rows);put32(dst+12,(uint32_t)sites);
    memcpy(dst+HEADER,src,prefix);memcpy(dst+HEADER+prefix,end-suffix,suffix);
    size_t out=HEADER+prefix+suffix;
    for(unsigned i=0;i<STREAMS;++i) {
        size_t size=(size_t)(s[i].at-s[i].start),coded=0;
        unsigned mode=0;
        if(size>=256) {
            coded=frontier_runs_encode(s[i].start,size,dst+out,cap-out);
            if(coded) mode=2;
            else {
                unsigned context=i==QUALITY?FRONTIER_QUALITY:i==BASES?FRONTIER_BYTES:FRONTIER_PLAIN;
                coded=frontier_entropy_encode(s[i].start,size,context,dst+out,cap-out);
                if(coded) mode=1;
            }
        }
        if(!mode) {
            if(size>cap-out) goto done;
            memcpy(dst+out,s[i].start,size);coded=size;
        }
        uint8_t *info=dst+16+9*i;
        info[0]=(uint8_t)mode;put32(info+1,(uint32_t)size);put32(info+5,(uint32_t)coded);
        out+=coded;
    }
    if(out<n) result=out;
done:
    free(storage);return result;
}

static int take(Reader *r,uint8_t *dst,size_t n) {
    if(n>(size_t)(r->end-r->at)) return 0;
    if(n) memcpy(dst,r->at,n);
    r->at+=n;return 1;
}
static size_t restore_header(Reader *m, Reader *text, Slice prev,size_t *last,uint8_t *out,size_t cap) {
    if(m->at==m->end) return 0;
    unsigned kind=*m->at++;
    if(kind==2 || kind==3) {
        if(kind==3 && !read_var(m,last)) return 0;
        return increment(prev,*last,out,cap);
    }
    if(kind==1) {
        if(!prev.len || prev.len>cap) return 0;
        memcpy(out,prev.data,prev.len);
        return prev.len;
    }
    size_t prefix,suffix,len;
    if(kind || !read_var(m,&prefix) || !read_var(m,&suffix) || !read_var(m,&len)) return 0;
    if(prefix>prev.len || suffix>prev.len-prefix || len>cap || prefix>cap-len || suffix>cap-len-prefix) return 0;
    if(prefix) memcpy(out,prev.data,prefix);
    if(!take(text,out+prefix,len)) return 0;
    if(suffix) memcpy(out+prefix+len,prev.data+prev.len-suffix,suffix);
    return prefix+len+suffix;
}
static void newline(uint8_t *p,unsigned ending) {
    if(ending==2) *p++='\r';
    *p='\n';
}

int frontier_fastq_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t raw) {
    if(n<HEADER || raw>(32u<<20)) return -1;
    size_t prefix=get32(src),suffix=get32(src+4),rows=get32(src+8),sites=get32(src+12);
    if(!rows || rows>raw/6 || sites>raw/2 || prefix>raw || suffix>raw-prefix ||
       prefix>n-HEADER || suffix>n-HEADER-prefix) return -1;
    size_t pos=HEADER+prefix+suffix;
    int result=-1;
    Reader streams[STREAMS]={0};
    size_t total=0;
    for(unsigned i=0;i<STREAMS;++i) {
        const uint8_t *info=src+16+9*i;
        size_t size=get32(info+1),coded=get32(info+5);
        unsigned mode=info[0];
        if(size>raw || coded>n-pos || mode>2 || (!mode && size!=coded)) goto done;
        total+=size;
        if(total>2*raw || (i==QUALITY && size!=sites) || (i==BASES && size>sites)) goto done;
        const uint8_t *data=src+pos;
        if(mode) {
            if(!size || coded>=size) goto done;
            uint8_t *buf=malloc(size);
            if(!buf) goto done;
            streams[i].owned=buf;
            int error=mode==1?frontier_entropy_decode(data,coded,buf,size):frontier_runs_decode(data,coded,buf,size);
            if(error) goto done;
            data=buf;
        }
        streams[i].at=data;streams[i].end=data+size;pos+=coded;
    }
    if(pos!=n) goto done;
    memcpy(dst,src+HEADER,prefix);memcpy(dst+raw-suffix,src+HEADER+prefix,suffix);
    size_t out=prefix,limit=raw-suffix,site=0,previous_len=0,last_digit=SIZE_MAX;
    Reader *m=&streams[META],*text=&streams[TEXT],*ex=&streams[EXCEPTIONS],*packed=&streams[BASES];
    size_t next_site=sites;
    if(ex->at<ex->end && (!read_var(ex,&next_site) || next_site>=sites)) goto done;
    Slice prev={NULL,0};
    for(size_t row=0;row<rows;++row) {
        if(m->at==m->end) goto done;
        unsigned flags=*m->at++,ending=1+(flags&1),plus=flags>>2;
        size_t len=previous_len;
        if(plus>2 || (!(flags&2) && !read_var(m,&len)) || !len || len>MAX_READ ||
           len>sites-site || 4*ending+2*len+2>limit-out) goto done;
        size_t head=restore_header(m,text,prev,&last_digit,dst+out,limit-out);
        if(!head || head>MAX_HEAD || head>limit-out-4*ending-2*len-1 || dst[out]!='@') goto done;
        prev=(Slice){dst+out,head};out+=head;newline(dst+out,ending);out+=ending;
        if((len+3)/4>(size_t)(packed->end-packed->at)) goto done;
        for(size_t i=0;i<len;++i) dst[out+i]=alphabet[(packed->at[i/4]>>(2*(i%4)))&3];
        packed->at+=(len+3)/4;
        while(next_site<site+len) {
            if(next_site<site || ex->at==ex->end) goto done;
            dst[out+next_site-site]=*ex->at++;
            if(ex->at==ex->end) next_site=sites;
            else {
                size_t delta;
                if(!read_var(ex,&delta) || delta>=sites-next_site-1) goto done;
                next_site+=delta+1;
            }
        }
        site+=len;out+=len;newline(dst+out,ending);out+=ending;
        dst[out++]='+';
        size_t remain=limit-out-len-2*ending;
        if(plus==1) {
            if(head-1>remain) goto done;
            memcpy(dst+out,prev.data+1,head-1);out+=head-1;
        } else if(plus==2) {
            size_t s;
            if(!read_var(m,&s) || s>remain || s>=MAX_HEAD || !take(text,dst+out,s)) goto done;
            out+=s;
        }
        newline(dst+out,ending);out+=ending;
        if(!take(&streams[QUALITY],dst+out,len)) goto done;
        out+=len;newline(dst+out,ending);out+=ending;
        previous_len=len;
    }
    if(site!=sites || next_site!=sites || out!=limit) goto done;
    for(unsigned i=0;i<STREAMS;++i) if(streams[i].at!=streams[i].end) goto done;
    result=0;
done:
    for(unsigned i=0;i<STREAMS;++i) free(streams[i].owned);
    return result;
}
