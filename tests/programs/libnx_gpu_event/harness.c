#define main referenceEventBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/channel.h"
#include "nvidia/address_space.h"
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 gpu_as_report[64] __attribute__((section(".data.gpu_as_report")))={0};
int main(int argc,char** argv){
    const int result=referenceEventBaseMain(argc,argv);
    for(unsigned i=0;i<2;++i){
        volatile u64* r=gpu_as_report+i*32;NvAddressSpace space={0};NvChannel channel={0};Event event={0};nvioctl_fence fence={0};u64 object=0;
        r[0]=nvInitialize();
        if(!r[0]){
            r[1]=nvAddressSpaceCreate(&space,0x10000);r[2]=space.fd;
            r[3]=nvMapInit();r[4]=nvMapGetFd();
            if(!r[1] && !r[3]){
                r[5]=nvChannelCreate(&channel,"/dev/nvhost-gpu");r[6]=channel.fd;
                if(!r[5]){
                    r[7]=nvioctlNvhostAsGpu_BindChannel(space.fd,channel.fd);
                    if(!r[7])r[8]=nvioctlChannel_AllocGpfifoEx2(channel.fd,0x800,1,0,0,0,0,&fence);
                    r[9]=fence.id;r[10]=fence.value;
                    if(!r[7] && !r[8])r[17]=nvioctlChannel_AllocObjCtx(channel.fd,0xB197,0,&object);
                    r[18]=object;
                    if(!r[7] && !r[8] && !r[17])r[19]=nvQueryEvent(channel.fd,NvEventId_Gpu_ErrorNotifier,&event);
                    r[20]=eventActive(&event);r[21]=event.revent;
                    if(eventActive(&event)){
                        r[22]=nvioctlChannel_SetErrorNotifier(channel.fd,1);
                        r[23]=nvioctlChannel_SetErrorNotifier(channel.fd,0);
                        r[24]=eventWait(&event,0);eventClose(&event);
                    }
                    r[25]=eventActive(&event);r[26]=event.revent;
                    nvChannelClose(&channel);r[11]=channel.fd;r[12]=channel.has_init;
                }
            }
            nvAddressSpaceClose(&space);r[13]=space.fd;r[14]=space.has_init;
            nvMapExit();r[15]=nvMapGetFd();nvExit();r[16]=nvGetServiceSession()->session;
        }
    }
    return result;
}
