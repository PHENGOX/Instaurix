typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned long usize;
typedef struct { u32 key,flags;u64 value[2]; } Config;
extern u64 relocated_slot;
extern u64 thread_id(u64 handle);
extern void return_to_loader(u64 result) __attribute__((noreturn));
volatile u64 report[8] __attribute__((section(".data.report")))={0};
unsigned char bss_probe[4096] __attribute__((aligned(4096)));
void* memset(void* destination,int value,usize size)
{ unsigned char* p=destination;for(usize i=0;i<size;++i) p[i]=(unsigned char)value;return destination; }
void diagAbortWithResult(u32 result) { return_to_loader(result); }
u64 boot_main(const Config* env,u64 sentinel,u64 lr,u64 sp)
{
    report[1]=(usize)env;report[2]=sentinel;report[3]=lr;report[4]=sp;
    if(sentinel!=~0ULL || (sp&15) || !env || !lr) return 1;
    if(relocated_slot!=(usize)report) return 2;
    for(usize i=0;i<sizeof bss_probe;++i) if(bss_probe[i]) return 3;
    u64 handle=0,heap=0,size=0;unsigned n=0;
    for(;n<16 && env[n].key;++n) {
        if(env[n].flags) return 4;
        if(env[n].key==1) handle=env[n].value[0];
        else if(env[n].key==3) { heap=env[n].value[0];size=env[n].value[1]; }
        else return 5;
    }
    if(n==16 || !heap || size<4096 || thread_id(handle)!=1) return 6;
    *(volatile u64*)heap=0x12345678;
    *(volatile u64*)(heap+size-8)=0x87654321;
    report[5]=handle;report[6]=heap;report[7]=size;report[0]=1;
    return 42; /* Nonzero return deliberately proves propagation to the host. */
}
