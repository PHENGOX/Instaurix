#define main referenceNvBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/ioctl.h"
#include <stdlib.h>
#include <string.h>
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 nv_report[16] __attribute__((section(".data.nv_report")))={0};
volatile u64 map_report[16] __attribute__((section(".data.map_report")))={0};
int main(int argc,char** argv){
    const int result=referenceNvBaseMain(argc,argv);
    nv_report[0]=__nx_nv_transfermem_size;
    for(unsigned attempt=0;attempt<2;++attempt){
        const unsigned n=attempt*2, m=attempt*8;
        nv_report[1+n]=nvInitialize();nv_report[2+n]=nvGetServiceSession()->session;
        if(!nv_report[1+n]){
            nv_report[6+n]=nvMapInit();nv_report[7+n]=nvMapGetFd();
            if(!nv_report[6+n]){
                void* buffer=aligned_alloc(0x1000,0x1000);map_report[m]=buffer!=NULL;
                if(buffer){
                    NvMap object={0};memset(buffer,0x42,0x1000);
                    const Result created=nvMapCreate(&object,buffer,0x1000,0x1000,NvKind_Pitch,true);
                    nv_report[10+n]=created;nv_report[11+n]=object.handle;
                    map_report[m+1]=created;map_report[m+2]=*(u64*)buffer;map_report[m+5]=object.id;
                    const u32 handle=object.handle;
                    nvMapClose(&object);map_report[m+3]=object.has_init;
                    map_report[m+4]=nvioctlNvmap_Free(nvMapGetFd(),handle);
                    map_report[m+6]=object.cpu_addr==NULL && object.handle==UINT32_MAX;
                    free(buffer);
                }
            }
            nvMapExit();
        }
        nvExit();
    }
    nv_report[5]=1;return result;
}