#define main referenceAsBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/address_space.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[32] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceAsBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* report=gpu_as_report+i*16;NvAddressSpace space={0};
        report[0]=nvInitialize();
        if(!report[0]){
            report[1]=nvAddressSpaceCreate(&space,0x10000);
            report[2]=space.has_init;report[3]=space.fd;report[4]=space.page_size;
            nvAddressSpaceClose(&space);report[5]=space.has_init;report[6]=space.fd;
            nvAddressSpaceClose(&space);report[7]=space.has_init;
            nvExit();report[8]=nvGetServiceSession()->session;
        }
    }
    return result;
}
