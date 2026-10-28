#define main referencePacketsBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include <stdlib.h>
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/address_space.h"
#include "nvidia/gpu_channel.h"
#include "runtime/hosversion.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[128] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referencePacketsBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* r=gpu_as_report+i*64;NvAddressSpace space={0};NvGpuChannel channel={0};NvMap map={0};
        r[0]=nvInitialize();if(r[0])continue;
        r[1]=nvAddressSpaceCreate(&space,0x10000);r[2]=space.fd;r[3]=nvMapInit();r[4]=nvMapGetFd();
        if(!r[1] && !r[3]){
            r[5]=nvGpuChannelCreate(&channel,&space,NvChannelPriority_Medium);r[6]=channel.base.has_init;r[7]=channel.base.fd;
            r[8]=channel.fence.id;r[9]=channel.fence.value;r[10]=channel.object_id;r[11]=eventActive(&channel.error_event);r[12]=channel.error_event.revent;
            if(!r[5]){
                u32* bytes=aligned_alloc(0x1000,0x10000);iova_t va=0;r[32]=(u64)bytes;
                if(bytes){
                    /* NVIDIA B06F INC SET_REFERENCE followed by DMA_NOP: decode-only reference. */
                    bytes[0]=0x20010014;bytes[1]=0x12345678;bytes[2]=0;r[33]=nvMapCreate(&map,bytes,0x10000,0x10000,NvKind_Pitch,true);r[34]=map.handle;
                    if(!r[33]){
                        r[35]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&va);r[36]=va;
                        if(!r[35]){
                            r[26]=nvGpuChannelAppendEntry(&channel,va,3,0,0);r[27]=channel.entries[0].desc;r[28]=channel.num_entries;
                            const u32 version=hosversionGet();hosversionSet(MAKEHOSVERSION(4,0,0));r[29]=nvGpuChannelKickoff(&channel);hosversionSet(version);
                            r[30]=channel.num_entries;r[31]=channel.fence_incr;r[37]=nvAddressSpaceUnmap(&space,va);
                        }
                        nvMapClose(&map);r[38]=map.has_init;
                    }
                    r[39]=bytes[0];r[40]=bytes[1];r[41]=bytes[2];free(bytes);
                }
                r[15]=eventWait(&channel.error_event,0);
                nvGpuChannelClose(&channel);r[16]=channel.base.has_init;r[17]=channel.base.fd;r[18]=eventActive(&channel.error_event);r[19]=channel.error_event.revent;
            }
        }
        nvAddressSpaceClose(&space);r[21]=space.fd;r[22]=space.has_init;nvMapExit();r[23]=nvMapGetFd();nvExit();r[24]=nvGetServiceSession()->session;
    }
    return result;
}
