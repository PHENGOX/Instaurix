#define main referenceCopyBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include <stdlib.h>
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/address_space.h"
#include "nvidia/gpu_channel.h"
#include "runtime/hosversion.h"
#include "arm/cache.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[128] __attribute__((section(".data.gpu_as_report")))={0};
static u8 pattern(unsigned i,unsigned lifetime){return (u8)((i*13+lifetime*29+3)^0xA5);}
static u32 hash(volatile const u8* data){u32 value=2166136261u;for(unsigned i=0;i<64;++i)value=(value^data[i])*16777619u;return value;}
static void commands(u32* words,iova_t source,iova_t destination,unsigned lifetime){
    /* Field-derived B197/B0B5 bindings, 3D no-ops and a single linear copy. */
    const u32 stream[16]={0x60010000,0xB197,0x60018000,0xB0B5,0x80000040,
        0x20048100,(u32)(source>>32),(u32)source,(u32)(destination>>32),(u32)destination,
        0x20018106,64,0x818680C0,0x80000040,0x20010014,0xC0DE0000u|lifetime};
    for(unsigned i=0;i<16;++i)words[i]=stream[i];
}
static Result kickoff(NvGpuChannel* channel){
    const u32 version=hosversionGet();hosversionSet(MAKEHOSVERSION(4,0,0));
    const Result result=nvGpuChannelKickoff(channel);hosversionSet(version);return result;
}
int main(int argc,char** argv){
    /* The inherited startup probe deliberately returns 77 on success. */
    const int base=referenceCopyBaseMain(argc,argv);int failed=base!=77;
    for(unsigned lifetime=0;lifetime<2;++lifetime){
        volatile u64* r=gpu_as_report+lifetime*64;NvAddressSpace space={0};NvGpuChannel channel={0};NvMap map={0};
        r[39]=r[40]=r[41]=~(u64)0;
        r[0]=nvInitialize();if(r[0]){failed=1;continue;}
        r[1]=nvAddressSpaceCreate(&space,0x10000);r[2]=space.fd;r[3]=nvMapInit();r[4]=nvMapGetFd();
        if(!r[1] && !r[3]){
            r[5]=nvGpuChannelCreate(&channel,&space,NvChannelPriority_Medium);r[6]=channel.base.has_init;r[7]=channel.base.fd;
            r[8]=channel.fence.id;r[9]=channel.fence.value;r[10]=channel.object_id;r[11]=eventActive(&channel.error_event);r[12]=channel.error_event.revent;
            if(!r[5]){
                u8* bytes=aligned_alloc(0x1000,0x10000);iova_t va=0;r[32]=(u64)bytes;
                if(bytes){
                    volatile u8* source=bytes+0x4000;volatile u8* destination=bytes+0x6000;
                    for(unsigned i=0;i<64;++i){source[i]=pattern(i,lifetime);destination[i]=0xCC;}
                    bytes[0x5FFF]=0xE1;bytes[0x6040]=0xD2;
                    r[33]=nvMapCreate(&map,bytes,0x10000,0x10000,NvKind_Pitch,true);r[34]=map.handle;
                    if(!r[33]){
                        r[35]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&va);r[36]=va;
                        if(!r[35]){
                            commands((u32*)bytes,va+0x4000,va+0x6000,lifetime);armDCacheFlush(bytes,64);r[46]=va+0x4000;r[47]=va+0x6000;r[48]=16;
                            r[26]=nvGpuChannelAppendEntry(&channel,va,16,0,0);r[27]=channel.entries[0].desc;r[28]=channel.num_entries;
                            r[29]=kickoff(&channel);r[30]=channel.num_entries;r[31]=channel.fence_incr;
                            /* This fixture targets our synchronous HLE profile, not native GPU completion timing. */
                            r[39]=r[40]=r[41]=0;
                            for(unsigned i=0;i<64;++i){if(source[i]!=pattern(i,lifetime))++r[39];if(destination[i]!=pattern(i,lifetime))++r[40];}
                            if(bytes[0x5FFF]!=0xE1)++r[41];if(bytes[0x6040]!=0xD2)++r[41];
                            r[42]=64;r[43]=hash(source);r[44]=hash(destination);
                            if(r[26] || r[29] || r[39] || r[40] || r[41])failed=1;
                            /* The bounded local profile must refuse overlapping spans without disturbing data. */
                            commands((u32*)bytes,va+0x4000,va+0x4001,lifetime);armDCacheFlush(bytes,64);
                            r[49]=nvGpuChannelAppendEntry(&channel,va,16,0,0);r[50]=kickoff(&channel);r[51]=channel.num_entries;
                            for(unsigned i=0;i<64;++i)if(source[i]!=pattern(i,lifetime) || destination[i]!=pattern(i,lifetime))++r[52];
                            if(r[49] || !r[50] || r[51]!=1 || r[52])failed=1;
                            channel.num_entries=0;r[53]=channel.num_entries;r[37]=nvAddressSpaceUnmap(&space,va);
                            if(r[37])failed=1;
                        }else failed=1;
                        nvMapClose(&map);r[38]=map.has_init;
                    }else failed=1;
                    free(bytes);
                }else failed=1;
                r[15]=eventWait(&channel.error_event,0);nvGpuChannelClose(&channel);
                r[16]=channel.base.has_init;r[17]=channel.base.fd;r[18]=eventActive(&channel.error_event);r[19]=channel.error_event.revent;
            }else failed=1;
        }else failed=1;
        nvAddressSpaceClose(&space);r[21]=space.fd;r[22]=space.has_init;nvMapExit();r[23]=nvMapGetFd();nvExit();r[24]=nvGetServiceSession()->session;
        r[45]=failed;
    }
    return failed;
}
