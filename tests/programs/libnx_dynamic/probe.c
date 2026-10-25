/* Test harness only: the relocation algorithm is the unchanged upstream source. */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned long usize;
extern void __nx_dynamic(usize base, const void* mod0);
extern void stop_probe(u64 result) __attribute__((noreturn));

/* Fixed report address supplied by link.ld. */
volatile u64 report[8] __attribute__((section(".data.report"))) = {0};
static union { u64 alignment; unsigned char bytes[512]; } module;

void* memset(void* destination, int value, usize size)
{
    unsigned char* p = destination;
    for (usize i=0; i<size; ++i) p[i]=(unsigned char)value;
    return destination;
}
void diagAbortWithResult(u32 result)
{
    report[0]=2; report[1]=result;
    stop_probe(result);
}
static void put64(usize offset,u64 value) { *(u64*)(module.bytes+offset)=value; }
static void put32(usize offset,u32 value) { *(u32*)(module.bytes+offset)=value; }

u64 probe_main(u64 mode)
{
    unsigned char* m=module.bytes;
    usize base=(usize)m;
    memset(m,0,512);
    report[2]=base;
    put32(0, mode==1 ? 0 : 0x30444f4d);
    put32(4,288); put32(8,256); put32(12,272);
    put64(288,7); put64(296,128);     /* DT_RELA */
    put64(304,8); put64(312,48);      /* DT_RELASZ */
    put64(320,36); put64(328,176);    /* DT_RELR */
    put64(336,35); put64(344,16);     /* DT_RELRSZ */
    put64(352,0); put64(360,0);
    put64(128,248); put64(136,mode==2 ? 257 : 1027); put64(144,400);
    put64(152,240); put64(160,0); put64(168,999); /* R_AARCH64_NONE */
    put64(176,192); put64(184,11); /* direct slot 0, bitmap slots 1 and 3 */
    put64(192,10); put64(200,20); put64(208,30); put64(216,40);
    put64(240,0x12345678); put64(248,0);
    put64(256,~0ULL); put64(264,~0ULL);
    __nx_dynamic(base,m);
    if(mode==1) {
        if(*(u64*)(m+256)!=~0ULL || *(u64*)(m+192)!=10 || *(u64*)(m+248)!=0) return 11;
        report[0]=1; return 0;
    }
    if(*(u64*)(m+256) || *(u64*)(m+264)) return 12;
    if(*(u64*)(m+248)!=base+400 || *(u64*)(m+240)!=0x12345678) return 13;
    if(*(u64*)(m+192)!=base+10 || *(u64*)(m+200)!=base+20 ||
       *(u64*)(m+208)!=30 || *(u64*)(m+216)!=base+40) return 14;
    report[0]=1; report[3]=*(u64*)(m+248); report[4]=*(u64*)(m+216);
    return 0;
}
