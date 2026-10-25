typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char u8;
typedef __UINTPTR_TYPE__ uptr;
extern u32 client_connect(const char*,u32*);
extern u32 client_send(u32);
extern u32 client_close_handle(u32);
extern void* client_tls(void);
extern void client_sleep(u64);
volatile struct Report { u64 status,loaded,presented,hash; } report={99,0,0,0};
static volatile u32* m;
static volatile u8 sprite[272], pixels[160*96*3];
static void clear(void) { for(u32 i=0;i<64;++i) m[i]=0; }
static void word64(u32 at,u64 v) { m[at]=(u32)v; m[at+1]=(u32)(v>>32); }
static u64 read64(u32 at) { return (u64)m[at]|((u64)m[at+1]<<32); }
static int reply(u32 h) { return client_send(h)==0 && m[4]==0x4F434653 && m[6]==0; }
static void descriptor(uptr address,u32 size) {
    m[2]=size; m[3]=(u32)address; m[4]=(u32)(((u64)address>>36)<<2)|(u32)((((u64)address>>32)&15)<<28);
}
static u32 get_service(u32 sm,const char* name) {
    clear(); m[0]=4; m[1]=10; m[4]=0x49434653; m[6]=1;
    for(u32 i=0;i<8 && name[i];++i) ((volatile u8*)m)[32+i]=(u8)name[i];
    if(!reply(sm) || m[2]!=32) return 0;
    return m[3];
}
void work(void) {
    u32 sm=0;
    m=(volatile u32*)client_tls(); report.status=2;
    if(client_connect("sm:",&sm)) return;
    clear(); m[0]=4; m[1]=0x8000000A; m[2]=1; m[8]=0x49434653;
    if(!reply(sm)) return;
    u32 files=get_service(sm,"emu:fs"), display=get_service(sm,"emu:disp");
    if(!files || !display || client_close_handle(sm)) return;
    report.status=3;
    clear(); m[0]=0x00100004; m[1]=8; descriptor((uptr)"sprite.txt",11); m[8]=0x49434653;
    if(!reply(files) || m[2]!=32) return;
    u32 file=m[3];
    if(client_close_handle(files)) return;
    clear(); m[0]=0x01000004; m[1]=12; descriptor((uptr)sprite,272); m[8]=0x49434653;
    word64(12,0); word64(14,272);
    if(!reply(file) || read64(8)!=272 || client_close_handle(file)) return;
    report.loaded=272;
    for(u32 y=0;y<16;++y) {
        for(u32 x=0;x<16;++x) if(sprite[y*17+x]!='.' && sprite[y*17+x]!='#') return;
        if(sprite[y*17+16]!='\n') return;
    }
    report.status=4;
    for(u32 frame=0;frame<8;++frame) {
    const u32 left=8+frame*12;
    u32 hash=2166136261U;
    for(u32 y=0;y<96;++y) for(u32 x=0;x<160;++x) {
        u8 r=(u8)(16+x/4), g=(u8)(24+y/3), b=(u8)(48+(x+y)/6);
        if(((x*13+y*7+frame*11)%113)==0) { r=140; g=170; b=210; }
        if(x>=left && x<left+64 && y>=16 && y<80 && sprite[((y-16)/4)*17+(x-left)/4]=='#') {
            r=(u8)(240-(y-16)); g=(u8)(120+(x-left)); b=(u8)(40+(y-16)*2);
        }
        const u32 at=(y*160+x)*3;
        pixels[at]=r; pixels[at+1]=g; pixels[at+2]=b;
        hash=(hash^r)*16777619U; hash=(hash^g)*16777619U; hash=(hash^b)*16777619U;
    }
    report.hash=hash;
    clear(); m[0]=0x00100004; m[1]=10; descriptor((uptr)pixels,sizeof(pixels));
    m[8]=0x49434653; m[12]=160; m[13]=96;
    if(!reply(display) || read64(8)!=sizeof(pixels)) return;
    report.presented=frame+1;
    if(frame+1<8) client_sleep(100000000);
    }
    if(client_close_handle(display)) return;
    report.status=1;
}
