typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
typedef __UINTPTR_TYPE__ uptr;
extern u32 client_connect(const char*,u32*);
extern u32 client_send(u32);
extern u32 client_close_handle(u32);
extern void* client_tls(void);

volatile struct Report {
    u64 status, sum, copied, hash, pointer_size, time, flags;
} report = {99,0,0,0,0,0,0};
static volatile u8 source[512], destination[512];
static volatile u32* message;

static void clear(void) { for(u32 i=0;i<64;++i) message[i]=0; }
static void word64(u32 at,u64 v) { message[at]=(u32)v; message[at+1]=(u32)(v>>32); }
static u64 read64(u32 at) { return (u64)message[at] | ((u64)message[at+1]<<32); }
static void request(u32 type,u32 words,u32 command) {
    clear(); message[0]=type; message[1]=words; message[4]=0x49434653; message[6]=command;
}
static int reply(u32 handle) {
    return client_send(handle)==0 && message[4]==0x4F434653 && message[6]==0;
}
static u32 clone(u32 handle) {
    request(5,8,2);
    if(!reply(handle) || message[1]!=0x80000008 || message[2]!=32) return 0;
    return message[3];
}
static int close_session(u32 handle) {
    clear(); message[0]=2;
    if(client_send(handle)!=0 || client_send(handle)!=0x15FF) return 0;
    return client_close_handle(handle)==0;
}
static void descriptor(u32 at,uptr address,u64 size) {
    message[at]=(u32)size; message[at+1]=(u32)address;
    message[at+2]=(u32)(((u64)address>>36)<<2) | (u32)(((size>>32)&15)<<24) |
        (u32)((((u64)address>>32)&15)<<28);
}

void work(void) {
    u32 sm=0;
    message=(volatile u32*)client_tls();
    report.status=2;
    if(client_connect("sm:",&sm)) return;
    u32 sm_clone=clone(sm);
    if(!sm_clone) return;
    clear(); message[0]=4; message[1]=0x8000000A; message[2]=1; message[8]=0x49434653;
    if(!reply(sm)) return;
    if(!close_session(sm)) return;
    report.flags=1;
    report.status=3;
    request(4,10,1);
    const char* name="emu:demo";
    for(u32 i=0;i<8;++i) ((volatile u8*)message)[32+i]=(u8)name[i];
    if(!reply(sm_clone) || message[2]!=32) return;
    u32 demo=message[3], demo_clone=clone(demo);
    if(!demo_clone || !close_session(demo)) return;
    report.flags|=2;
    report.status=4;
    request(5,8,3);
    if(!reply(demo_clone) || message[1]!=9 || (message[8]&0xFFFF)!=0) return;
    report.pointer_size=message[8]&0xFFFF;
    request(4,12,0); word64(8,20); word64(10,35);
    if(!reply(demo_clone) || read64(8)!=55) return;
    report.sum=read64(8);
    report.status=5;
    for(u32 i=0;i<512;++i) source[i]=(u8)(i*17+3);
    clear(); message[0]=0x01100004; message[1]=8;
    descriptor(2,(uptr)source,512); descriptor(5,(uptr)destination,512);
    message[8]=0x49434653; message[10]=2;
    if(!reply(demo_clone) || read64(8)!=512) return;
    report.copied=read64(8);
    u32 hash=2166136261U;
    for(u32 i=0;i<512;++i) {
        u8 expected=(u8)(i*17+3);
        if(destination[i]!=expected) return;
        hash=(hash^destination[i])*16777619U;
    }
    report.hash=hash;
    report.status=6;
    request(4,8,1);
    if(!reply(demo_clone)) return;
    report.time=read64(8);
    if(!close_session(sm_clone) || !close_session(demo_clone)) return;
    report.flags|=4;
    report.status=1;
}
