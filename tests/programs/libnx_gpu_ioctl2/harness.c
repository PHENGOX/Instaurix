#define main referenceIoctl2BaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/address_space.h"
#include "nvidia/gpu_channel.h"
#include "runtime/hosversion.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[64] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceIoctl2BaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* r=gpu_as_report+i*32;NvAddressSpace space={0};NvGpuChannel channel={0};
        const NvChannelPriority priority=i?NvChannelPriority_High:NvChannelPriority_Medium;r[25]=priority;
        r[0]=nvInitialize();
        if(!r[0]){
            r[1]=nvAddressSpaceCreate(&space,0x10000);r[2]=space.fd;
            r[3]=nvMapInit();r[4]=nvMapGetFd();
            if(!r[1] && !r[3]){
                r[5]=nvGpuChannelCreate(&channel,&space,priority);
                r[6]=channel.base.has_init;r[7]=channel.base.fd;
                r[8]=channel.fence.id;r[9]=channel.fence.value;r[10]=channel.object_id;
                r[11]=eventActive(&channel.error_event);r[12]=channel.error_event.revent;
                r[13]=channel.fence_incr;r[14]=channel.num_entries;
                if(!r[5]){
                    r[15]=eventWait(&channel.error_event,0);
                    /* Synthetic unmapped VA: transport validation only, never executable work. */
                    r[26]=nvGpuChannelAppendEntry(&channel,(iova_t)0x100000000ULL,1,0,0);
                    r[27]=channel.entries[0].desc;r[28]=channel.num_entries;
                    const u32 savedVersion=hosversionGet();
                    hosversionSet(MAKEHOSVERSION(4,0,0)); /* Select the original ioctl2 branch only. */
                    r[29]=nvGpuChannelKickoff(&channel);
                    hosversionSet(savedVersion);
                    r[30]=channel.num_entries;r[31]=channel.fence_incr;
                }
                nvGpuChannelClose(&channel);r[16]=channel.base.has_init;r[17]=channel.base.fd;
                r[18]=eventActive(&channel.error_event);r[19]=channel.error_event.revent;
                nvGpuChannelClose(&channel);r[20]=channel.base.has_init;
            }
            nvAddressSpaceClose(&space);r[21]=space.fd;r[22]=space.has_init;
            nvMapExit();r[23]=nvMapGetFd();nvExit();r[24]=nvGetServiceSession()->session;
        }
    }
    return result;
}
