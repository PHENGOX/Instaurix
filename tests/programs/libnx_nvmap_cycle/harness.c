#define main referenceNvBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/ioctl.h"
#include <stdlib.h>
#include <string.h>
volatile u64 map_report[16] __attribute__((section(".data.map_report")))={0};
// Explicit small-memory test profile; original SDK functions are unchanged.
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 nv_report[16] __attribute__((section(".data.nv_report")))={0};
int main(int argc,char** argv){
    const int result=referenceNvBaseMain(argc,argv);
    nv_report[0]=__nx_nv_transfermem_size;
    nv_report[1]=nvInitialize();nv_report[2]=nvGetServiceSession()->session;
    if(!nv_report[1]){nv_report[6]=nvMapInit();nv_report[7]=nvMapGetFd();if(!nv_report[6]){u32 handle=0xBADF00D;nv_report[10]=nvioctlNvmap_Create(nvMapGetFd(),0x1000,&handle);nv_report[11]=handle;if(!nv_report[10]){
    void* buffer=aligned_alloc(0x1000,0x1000);map_report[0]=buffer!=NULL;
    if(buffer){memset(buffer,0x42,0x1000);map_report[1]=nvioctlNvmap_Alloc(nvMapGetFd(),handle,0,1,0x1000,0,buffer);map_report[2]=*(u64*)buffer;}
    map_report[3]=nvioctlNvmap_Free(nvMapGetFd(),handle);
    map_report[4]=nvioctlNvmap_Free(nvMapGetFd(),handle);free(buffer);
}}nvMapExit();}
    nvExit();
    nv_report[3]=nvInitialize();nv_report[4]=nvGetServiceSession()->session;
    if(!nv_report[3]){nv_report[8]=nvMapInit();nv_report[9]=nvMapGetFd();if(!nv_report[8]){u32 handle=0xBADF00D;nv_report[12]=nvioctlNvmap_Create(nvMapGetFd(),0x1000,&handle);nv_report[13]=handle;if(!nv_report[12]){
    void* buffer=aligned_alloc(0x1000,0x1000);map_report[8]=buffer!=NULL;
    if(buffer){memset(buffer,0x42,0x1000);map_report[9]=nvioctlNvmap_Alloc(nvMapGetFd(),handle,0,1,0x1000,0,buffer);map_report[10]=*(u64*)buffer;}
    map_report[11]=nvioctlNvmap_Free(nvMapGetFd(),handle);
    map_report[12]=nvioctlNvmap_Free(nvMapGetFd(),handle);free(buffer);
}}nvMapExit();}
    nvExit();nv_report[5]=1;
    return result;
}