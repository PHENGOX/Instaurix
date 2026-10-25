typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
typedef __UINTPTR_TYPE__ uptr;
extern u32 client_connect(const char*,u32*);
extern u32 client_send(u32);
extern u32 client_close_handle(u32);
extern void* client_tls(void);
volatile struct Report { u64 status,size,read,hash,partial,eof; } report={99,0,0,0,0,99};
static volatile u32* m;
static volatile u8 bytes[128];
static void clear(void) { for(u32 i=0;i<64;++i) m[i]=0; }
static void word64(u32 at,u64 v) { m[at]=(u32)v; m[at+1]=(u32)(v>>32); }
static u64 read64(u32 at) { return (u64)m[at]|((u64)m[at+1]<<32); }
static int reply(u32 h) { return client_send(h)==0 && m[4]==0x4F434653 && m[6]==0; }
static void request(u32 command) { clear(); m[0]=4; m[1]=8; m[4]=0x49434653; m[6]=command; }
static void descriptor(uptr address,u32 size) {
    m[2]=size; m[3]=(u32)address; m[4]=(u32)(((u64)address>>36)<<2)| (u32)((((u64)address>>32)&15)<<28);
}
static int read_file(u32 handle,u64 offset,u64 count) {
    clear(); m[0]=0x01000004; m[1]=12;
    descriptor((uptr)bytes,128);
    m[8]=0x49434653; word64(12,offset); word64(14,count);
    return reply(handle);
}
void work(void) {
    const char expected[]="Switch2Emu read-only fixture.\n";
    u32 sm=0;
    m=(volatile u32*)client_tls(); report.status=2;
    if(client_connect("sm:",&sm)) return;
    clear(); m[0]=4; m[1]=0x8000000A; m[2]=1; m[8]=0x49434653;
    if(!reply(sm)) return;
    request(1); m[1]=10;
    const char* service="emu:fs";
    for(u32 i=0;i<6;++i) ((volatile u8*)m)[32+i]=(u8)service[i];
    if(!reply(sm) || m[2]!=32) return;
    u32 fs=m[3];
    if(client_close_handle(sm)) return;
    report.status=3;
    clear(); m[0]=0x00100004; m[1]=8;
    descriptor((uptr)"sample.txt",11); m[8]=0x49434653;
    if(!reply(fs) || m[2]!=32) return;
    u32 file=m[3];
    if(client_close_handle(fs)) return;
    request(1);
    if(!reply(file) || read64(8)!=sizeof(expected)-1) return;
    report.size=read64(8);
    report.status=4;
    for(u32 i=0;i<128;++i) bytes[i]=0xA5;
    if(!read_file(file,0,128) || read64(8)!=sizeof(expected)-1) return;
    report.read=read64(8);
    u32 hash=2166136261U;
    for(u32 i=0;i<sizeof(expected)-1;++i) {
        if(bytes[i]!=(u8)expected[i]) return;
        hash=(hash^bytes[i])*16777619U;
    }
    for(u32 i=sizeof(expected)-1;i<128;++i) if(bytes[i]!=0xA5) return;
    report.hash=hash;
    report.status=5;
    clear(); m[0]=5; m[1]=8; m[4]=0x49434653; m[6]=2;
    if(!reply(file) || m[2]!=32) return;
    u32 clone=m[3];
    if(client_close_handle(file)) return;
    if(!read_file(clone,7,9) || read64(8)!=9) return;
    for(u32 i=0;i<9;++i) if(bytes[i]!=(u8)expected[i+7]) return;
    report.partial=read64(8);
    bytes[0]=0x7F;
    if(!read_file(clone,report.size,128) || read64(8)!=0 || bytes[0]!=0x7F) return;
    report.eof=read64(8);
    if(client_close_handle(clone)) return;
    report.status=1;
}
