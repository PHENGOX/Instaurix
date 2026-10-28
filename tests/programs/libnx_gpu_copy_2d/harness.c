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
static u32 hash(volatile const u8* data,unsigned width,unsigned rows,unsigned pitch){u32 value=2166136261u;for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<width;++x)value=(value^data[y*pitch+x])*16777619u;return value;}
static void commands(u32* words,iova_t source,iova_t destination,unsigned lifetime,unsigned width,unsigned rows,unsigned sourcePitch,unsigned destinationPitch){
    /* Pitch-to-pitch multiline transfer; original SDK transport is unchanged. */
    const u32 stream[18]={0x60010000,0xB197,0x60018000,0xB0B5,0x80000040,
        0x20088100,(u32)(source>>32),(u32)source,(u32)(destination>>32),(u32)destination,
        sourcePitch,destinationPitch,width,rows,0x838680C0,0x80000040,0x20010014,0xC02D0000u|lifetime};
    for(unsigned i=0;i<18;++i)words[i]=stream[i];
}
static Result kickoff(NvGpuChannel* channel){
    const u32 version=hosversionGet();hosversionSet(MAKEHOSVERSION(4,0,0));
    const Result result=nvGpuChannelKickoff(channel);hosversionSet(version);return result;
}
int main(int argc,char** argv){
    /* The inherited startup probe deliberately returns 77 on success. */
    const int base=referenceCopyBaseMain(argc,argv);int failed=base!=77;
    for(unsigned lifetime=0;lifetime<2;++lifetime){
        const unsigned width=lifetime?17:7,rows=lifetime?4:3,sourcePitch=lifetime?31:13,destinationPitch=lifetime?48:64;
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
                    for(unsigned i=0;i<256;++i){source[i]=pattern(i,lifetime);destination[i]=0xCC;}
                    bytes[0x5FFF]=0xE1;bytes[0x6100]=0xD2;
                    r[33]=nvMapCreate(&map,bytes,0x10000,0x10000,NvKind_Pitch,true);r[34]=map.handle;
                    if(!r[33]){
                        r[35]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&va);r[36]=va;
                        if(!r[35]){
                            commands((u32*)bytes,va+0x4000,va+0x6000,lifetime,width,rows,sourcePitch,destinationPitch);armDCacheFlush(bytes,72);r[46]=va+0x4000;r[47]=va+0x6000;r[48]=18;
                            r[54]=width;r[55]=rows;r[56]=sourcePitch;r[57]=destinationPitch;r[58]=256-width*rows;
                            r[26]=nvGpuChannelAppendEntry(&channel,va,18,0,0);r[27]=channel.entries[0].desc;r[28]=channel.num_entries;
                            r[29]=kickoff(&channel);r[30]=channel.num_entries;r[31]=channel.fence_incr;
                            /* This fixture targets our synchronous HLE profile, not native GPU completion timing. */
                            r[39]=r[40]=r[41]=0;
                            for(unsigned i=0;i<256;++i){
                                if(source[i]!=pattern(i,lifetime))++r[39];
                                const unsigned y=i/destinationPitch,x=i%destinationPitch;
                                if(y<rows && x<width){if(destination[i]!=pattern(y*sourcePitch+x,lifetime))++r[40];}
                                else if(destination[i]!=0xCC)++r[41];
                            }
                            if(bytes[0x5FFF]!=0xE1)++r[41];if(bytes[0x6100]!=0xD2)++r[41];
                            r[42]=width*rows;r[43]=hash(source,width,rows,sourcePitch);r[44]=hash(destination,width,rows,destinationPitch);
                            if(r[26] || r[29] || r[39] || r[40] || r[41])failed=1;
                            /* The bounded local profile must refuse overlapping spans without disturbing data. */
                            commands((u32*)bytes,va+0x4000,va+0x4001,lifetime,width,rows,sourcePitch,destinationPitch);armDCacheFlush(bytes,72);
                            r[49]=nvGpuChannelAppendEntry(&channel,va,18,0,0);r[50]=kickoff(&channel);r[51]=channel.num_entries;
                            for(unsigned i=0;i<256;++i){
                                const unsigned y=i/destinationPitch,x=i%destinationPitch;
                                const u8 expected=y<rows && x<width?pattern(y*sourcePitch+x,lifetime):0xCC;
                                if(source[i]!=pattern(i,lifetime) || destination[i]!=expected)++r[52];
                            }
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
