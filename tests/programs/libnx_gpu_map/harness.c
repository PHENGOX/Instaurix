#define main referenceMapBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include <stdlib.h>
#include "services/nv.h"
#include "nvidia/address_space.h"
#include "nvidia/map.h"
#include "nvidia/ioctl.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[128] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceMapBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* report=gpu_as_report+i*64;NvAddressSpace space={0};NvMap map={0};
        report[0]=nvInitialize();if(report[0])continue;
        report[1]=nvAddressSpaceCreate(&space,0x10000);report[2]=space.fd;
        report[3]=nvMapInit();u64* bytes=aligned_alloc(0x1000,0x20000);report[6]=(u64)bytes;
        if(!report[1] && !report[3] && bytes){
            bytes[0]=0x1122334455667788ULL;bytes[0x2000]=0x8877665544332211ULL;bytes[0x3fff]=0x0123456789abcdefULL;
            report[4]=nvMapCreate(&map,bytes,0x20000,0x10000,NvKind_Pitch,true);report[5]=map.handle;
            if(!report[4]){
                iova_t a=UINT64_MAX,b=UINT64_MAX,fixed=UINT64_MAX,partial=UINT64_MAX,out=UINT64_MAX;
                report[7]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&a);report[8]=a;
                if(!report[7]){
                    report[9]=nvioctlNvmap_Free(nvMapGetFd(),map.handle);
                    report[10]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&b);report[11]=b;
                    report[12]=nvAddressSpaceUnmap(&space,a);report[13]=nvAddressSpaceUnmap(&space,a);
                    report[14]=nvioctlNvmap_Free(nvMapGetFd(),map.handle);
                    report[15]=nvAddressSpaceUnmap(&space,b);
                    report[16]=nvAddressSpaceAlloc(&space,false,0x20000,&fixed);report[17]=fixed;
                    report[18]=nvAddressSpaceMapFixed(&space,map.handle,true,NvKind_Pitch,fixed);
                    report[19]=nvAddressSpaceFree(&space,fixed,0x20000);
                    report[20]=nvAddressSpaceUnmap(&space,fixed);report[21]=nvAddressSpaceFree(&space,fixed,0x20000);
                    report[22]=nvAddressSpaceFree(&space,fixed,0x20000);
                    report[23]=nvAddressSpaceAlloc(&space,false,0x10000,&partial);report[24]=partial;
                    report[25]=nvioctlNvhostAsGpu_MapBufferEx(space.fd,NvMapBufferFlags_FixedOffset|NvMapBufferFlags_IsCacheable,NvKind_Pitch,map.handle,0x10000,0x10000,0x10000,partial,&out);
                    report[26]=nvAddressSpaceUnmap(&space,partial);report[27]=nvAddressSpaceFree(&space,partial,0x10000);
                    report[28]=nvAddressSpaceMap(&space,map.handle,true,NvKind_Pitch,&a);report[29]=a;
                }
                report[30]=bytes[0];report[31]=bytes[0x2000];report[32]=bytes[0x3fff];
            }
        }
        nvAddressSpaceClose(&space);report[33]=space.fd;report[37]=space.has_init;
        nvMapClose(&map);report[34]=map.has_init;free(bytes);
        if(!report[3])nvMapExit();report[35]=nvMapGetFd();nvExit();report[36]=nvGetServiceSession()->session;
        nvAddressSpaceClose(&space);report[38]=space.has_init;
    }
    return result;
}
