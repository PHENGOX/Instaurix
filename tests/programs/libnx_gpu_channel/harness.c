#define main referenceChannelBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/channel.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[32] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceChannelBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* r=gpu_as_report+i*16;NvChannel channel={0};
        r[0]=nvInitialize();
        if(!r[0]){
            r[1]=nvMapInit();r[2]=nvMapGetFd();
            if(!r[1]){
                r[3]=nvChannelCreate(&channel,"/dev/nvhost-gpu");
                r[4]=channel.has_init;r[5]=channel.fd;
                if(!r[3])r[6]=nvChannelSetPriority(&channel,NvChannelPriority_Medium);
                nvChannelClose(&channel);r[7]=channel.has_init;r[8]=channel.fd;
                nvChannelClose(&channel);r[9]=channel.has_init;
                nvMapExit();r[10]=nvMapGetFd();
            }
            nvExit();r[11]=nvGetServiceSession()->session;
        }
    }
    return result;
}
