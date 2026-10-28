#define main referenceFenceBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/fence.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 fence_report[16] __attribute__((section(".data.fence_report")))={0};
int main(int argc,char** argv){
    const int result=referenceFenceBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        unsigned at=i*8;
        fence_report[at]=nvInitialize();
        if(!fence_report[at]){
            fence_report[at+1]=nvFenceInit();fence_report[at+2]=nvFenceGetFd();
            if(!fence_report[at+1])nvFenceExit();
            fence_report[at+3]=nvFenceGetFd();nvExit();
            fence_report[at+4]=nvGetServiceSession()->session;
        }
    }
    return result;
}