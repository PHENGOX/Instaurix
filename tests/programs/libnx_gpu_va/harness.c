#define main referenceVaBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/address_space.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[64] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceVaBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* report=gpu_as_report+i*32;NvAddressSpace space={0};
        report[0]=nvInitialize();
        if(!report[0]){
            report[1]=nvAddressSpaceCreate(&space,0x10000);
            report[2]=space.has_init;report[3]=space.fd;report[4]=space.page_size;
            if(!report[1]){
                iova_t first=UINT64_MAX,second=UINT64_MAX,third=UINT64_MAX,left=UINT64_MAX;
                report[9]=nvAddressSpaceAlloc(&space,false,0x10001,&first);report[10]=first;
                if(!report[9]){
                    report[11]=nvAddressSpaceAlloc(&space,false,0x10000,&second);report[12]=second;
                    report[13]=nvAddressSpaceFree(&space,first,0x20000);
                    report[14]=nvAddressSpaceFree(&space,first,0x20000);
                    report[15]=nvAddressSpaceAlloc(&space,false,0x10000,&third);report[16]=third;
                    report[17]=nvAddressSpaceFree(&space,third,0x10000);
                    report[18]=nvAddressSpaceFree(&space,second,0x10000);
                    report[19]=nvAddressSpaceAllocFixed(&space,false,0x10000,0x100080000ULL);
                    report[20]=nvAddressSpaceAllocFixed(&space,false,0x10000,0x100080000ULL);
                    report[21]=nvAddressSpaceFree(&space,0x100080000ULL,0x10000);
                    report[22]=nvAddressSpaceAlloc(&space,false,0,&third);
                    report[23]=nvAddressSpaceFree(&space,0x100300000ULL,0x10000);
                    report[24]=nvAddressSpaceAlloc(&space,false,0x10000,&left);report[25]=left;
                }
            }
            nvAddressSpaceClose(&space);report[5]=space.has_init;report[6]=space.fd;
            nvAddressSpaceClose(&space);report[7]=space.has_init;
            nvExit();report[8]=nvGetServiceSession()->session;
        }
    }
    return result;
}
