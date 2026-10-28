#define main referenceNvBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
// Explicit small-memory test profile; original SDK functions are unchanged.
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 nv_report[16] __attribute__((section(".data.nv_report")))={0};
int main(int argc,char** argv){
    const int result=referenceNvBaseMain(argc,argv);
    nv_report[0]=__nx_nv_transfermem_size;
    nv_report[1]=nvInitialize();nv_report[2]=nvGetServiceSession()->session;
    if(!nv_report[1]){nv_report[6]=nvMapInit();nv_report[7]=nvMapGetFd();nvMapExit();}
    nvExit();
    nv_report[3]=nvInitialize();nv_report[4]=nvGetServiceSession()->session;
    if(!nv_report[3]){nv_report[8]=nvMapInit();nv_report[9]=nvMapGetFd();nvMapExit();}
    nvExit();nv_report[5]=1;
    return result;
}