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
static u8 pixel(unsigned x,unsigned y,unsigned lifetime){return (u8)((x*17+y*29+(x/64)*97+(y/8)*43+lifetime*31)^0xA5);}
static u8 physicalByte(unsigned i,unsigned width,unsigned lifetime){
    const unsigned gob=i/512,sector=(i%512)/16;
    const unsigned x=(gob%(width/64))*64+(sector/16)*32+((sector/2)%2)*16+i%16;
    const unsigned y=(gob/(width/64))*8+((sector/4)%4)*2+sector%2;
    return pixel(x,y,lifetime);
}
static u32 rowHash(volatile const u8* data,unsigned width,unsigned rows,unsigned pitch){u32 h=2166136261u;for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<width;++x)h=(h^data[y*pitch+x])*16777619u;return h;}
static u32 expectedHash(unsigned width,unsigned rows,unsigned lifetime){u32 h=2166136261u;for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<width;++x)h=(h^pixel(x,y,lifetime))*16777619u;return h;}
static u32 physicalHash(volatile const u8* data,unsigned size){u32 h=2166136261u;for(unsigned i=0;i<size;++i)h=(h^data[i])*16777619u;return h;}
static u8 destinationByte(unsigned i,unsigned width,unsigned pitch,unsigned lifetime){return i%pitch<width?pixel(i%pitch,i/pitch,lifetime):0xCC;}
static void commands(u32* words,iova_t source,iova_t destination,unsigned lifetime,unsigned width,unsigned rows,unsigned pitch){
    const u32 stream[25]={0x60010000,0xB197,0x60018000,0xB0B5,0x80000040,
        0x20088100,(u32)(source>>32),(u32)source,(u32)(destination>>32),(u32)destination,
        0,pitch,width,rows,0x200681CA,0x1000,width,rows,1,0,0,
        0x830680C0,0x80000040,0x20010014,0xC0B10000u|lifetime};
    for(unsigned i=0;i<25;++i)words[i]=stream[i];
}
static Result kickoff(NvGpuChannel* channel){
    const u32 version=hosversionGet();hosversionSet(MAKEHOSVERSION(4,0,0));
    const Result result=nvGpuChannelKickoff(channel);hosversionSet(version);return result;
}
int main(int argc,char** argv){
    /* The inherited startup probe deliberately returns 77 on success. */
    const int base=referenceCopyBaseMain(argc,argv);int failed=base!=77;
    for(unsigned lifetime=0;lifetime<2;++lifetime){
        const unsigned width=lifetime?64:128,rows=16,destinationPitch=lifetime?80:144;
        const unsigned sourceSize=width*rows,destinationSize=destinationPitch*rows;
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
                    for(unsigned i=0;i<sourceSize;++i)source[i]=physicalByte(i,width,lifetime);
                    for(unsigned i=0;i<destinationSize;++i)destination[i]=0xCC;
                    bytes[0x5FFF]=0xE1;bytes[0x6000+destinationSize]=0xD2;
                    r[33]=nvMapCreate(&map,bytes,0x10000,0x10000,NvKind_Pitch,true);r[34]=map.handle;
                    if(!r[33]){
                        r[35]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&va);r[36]=va;
                        if(!r[35]){
                            commands((u32*)bytes,va+0x4000,va+0x6000,lifetime,width,rows,destinationPitch);armDCacheFlush(bytes,100);
                            r[46]=va+0x4000;r[47]=va+0x6000;r[48]=25;
                            r[54]=width;r[55]=rows;r[56]=sourceSize;r[57]=destinationPitch;r[58]=destinationSize-width*rows;r[60]=destinationSize;r[61]=0x1000;
                            r[26]=nvGpuChannelAppendEntry(&channel,va,25,0,0);r[27]=channel.entries[0].desc;r[28]=channel.num_entries;
                            r[29]=kickoff(&channel);r[30]=channel.num_entries;r[31]=channel.fence_incr;
                            /* Output reads rely on the explicitly synchronous historical HLE policy. */
                            r[39]=r[40]=r[41]=0;
                            for(unsigned i=0;i<sourceSize;++i)if(source[i]!=physicalByte(i,width,lifetime))++r[39];
                            for(unsigned i=0;i<destinationSize;++i)if(destination[i]!=destinationByte(i,width,destinationPitch,lifetime)){
                                if(i%destinationPitch<width)++r[40];else ++r[41];
                            }
                            if(bytes[0x5FFF]!=0xE1)++r[41];if(bytes[0x6000+destinationSize]!=0xD2)++r[41];
                            r[42]=sourceSize;r[43]=expectedHash(width,rows,lifetime);r[44]=rowHash(destination,width,rows,destinationPitch);r[59]=physicalHash(source,sourceSize);
                            if(r[26] || r[29] || r[39] || r[40] || r[41])failed=1;
                            /* A second complete source conversion must refuse overlapping envelopes. */
                            commands((u32*)bytes,va+0x4000,va+0x4001,lifetime,width,rows,destinationPitch);armDCacheFlush(bytes,100);
                            r[49]=nvGpuChannelAppendEntry(&channel,va,25,0,0);r[50]=kickoff(&channel);r[51]=channel.num_entries;
                            for(unsigned i=0;i<sourceSize;++i)if(source[i]!=physicalByte(i,width,lifetime))++r[52];
                            for(unsigned i=0;i<destinationSize;++i)if(destination[i]!=destinationByte(i,width,destinationPitch,lifetime))++r[52];
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
