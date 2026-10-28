#define main referenceViBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/vi.h"
#include "display/native_window.h"
#include "kernel/svc.h"
#include "services/nv.h"
#include "nvidia/map.h"
#include "nvidia/ioctl.h"
#include "nvidia/graphic_buffer.h"
#include <stdlib.h>
#include <string.h>
u32 __nx_nv_transfermem_size=0x10000;
volatile u64 registration_report[16] __attribute__((section(".data.registration_report")))={0};
static void registrationProbe(NWindow* window){
    registration_report[0]=nvInitialize();
    if(registration_report[0])return;
    registration_report[1]=nvMapInit();
    if(!registration_report[1]){
        void* buffer=aligned_alloc(0x1000,0x1000);registration_report[2]=buffer!=NULL;
        if(buffer){
            memset(buffer,0x42,0x1000);NvMap map={0};
            registration_report[3]=nvMapCreate(&map,buffer,0x1000,0x1000,NvKind_Pitch,true);
            registration_report[4]=map.id;
            if(!registration_report[3]){
                NvGraphicBuffer graphic={0};
                graphic.header.num_ints=(sizeof(graphic)-sizeof(NativeHandle))/4;
                graphic.unk0=-1;graphic.nvmap_id=map.id;graphic.magic=0xDAFFCAFF;graphic.pid=42;
                graphic.usage=GRALLOC_USAGE_HW_COMPOSER|GRALLOC_USAGE_HW_RENDER|GRALLOC_USAGE_HW_TEXTURE;
                graphic.format=graphic.ext_format=PIXEL_FORMAT_RGBA_8888;graphic.stride=16;graphic.total_size=4096;graphic.num_planes=1;
                graphic.planes[0].width=16;graphic.planes[0].height=16;graphic.planes[0].color_format=NvColorFormat_A8B8G8R8;
                graphic.planes[0].layout=NvLayout_Pitch;graphic.planes[0].pitch=64;graphic.planes[0].kind=NvKind_Pitch;graphic.planes[0].size=4096;
                registration_report[5]=nwindowSetDimensions(window,16,16);
                registration_report[6]=nwindowConfigureBuffer(window,0,&graphic);
                registration_report[7]=window->slots_configured;
                registration_report[8]=eventWait(&window->event,0);
                registration_report[9]=*(u64*)buffer;
                registration_report[10]=sizeof(graphic);
                registration_report[13]=nwindowReleaseBuffers(window);
                registration_report[14]=window->slots_configured;
                const u32 handle=map.handle;
                nvMapClose(&map);registration_report[11]=map.has_init;
                registration_report[15]=nvioctlNvmap_Free(nvMapGetFd(),handle);
            }
            free(buffer);
        }
    }
    nvMapExit();nvExit();registration_report[12]=1;
}
volatile u64 window_report[32] __attribute__((section(".data.window_report")))={0};

volatile u64 vi_report[32] __attribute__((section(".data.vi_report")))={0};
int main(int argc,char** argv) {
    const int result=referenceViBaseMain(argc,argv);
    ViDisplay a,b;
    ViLayer layer;
    vi_report[0]=99;
    vi_report[1]=viInitialize(ViServiceType_Default);
    if(vi_report[1])return result;
    vi_report[2]=viGetSession_IApplicationDisplayService()->session;
    vi_report[3]=viGetSession_IHOSBinderDriverRelay()->session;
    vi_report[4]=viOpenDefaultDisplay(&a);vi_report[5]=a.display_id;
    vi_report[6]=viOpenDefaultDisplay(&b);vi_report[7]=b.display_id;
    s32 width=-1,height=-1;
    vi_report[8]=viGetDisplayResolution(&a,&width,&height);
    vi_report[9]=(u64)(s64)width;vi_report[10]=(u64)(s64)height;
    vi_report[11]=viCreateLayer(&a,&layer);
    vi_report[12]=layer.initialized;vi_report[13]=layer.layer_id;
    if(layer.initialized) {
        NWindow window;
        window_report[0]=nwindowCreateFromLayer(&window,&layer);
        window_report[1]=nwindowIsValid(&window);window_report[2]=window.is_connected;
        window_report[3]=window.slots_configured;window_report[4]=(u64)(s64)window.cur_slot;
        u32 w=0,h=0;
        window_report[5]=nwindowGetDimensions(&window,&w,&h);window_report[6]=w;window_report[7]=h;
        window_report[8]=eventActive(&window.event);
        window_report[9]=eventWait(&window.event,0);
        window_report[10]=eventClear(&window.event);
        window_report[11]=svcSignalEvent(window.event.revent);
        registrationProbe(&window);
        Event events[128];u32 count=0;Result last=0;bool distinct=true;
        for(;count<128;count++) {
            last=binderGetNativeHandle(&window.bq,0xF,&events[count]);
            if(R_FAILED(last))break;
            if(!events[count].revent || events[count].revent==window.event.revent)distinct=false;
            for(u32 j=0;j<count;j++)if(events[j].revent==events[count].revent)distinct=false;
        }
        window_report[13]=count;window_report[14]=last;window_report[15]=distinct;
        window_report[16]=count?eventWait(&events[0],0):0;
        for(u32 i=0;i<count;i++)eventClose(&events[i]);
        Event retry;window_report[17]=binderGetNativeHandle(&window.bq,0xF,&retry);
        if(R_SUCCEEDED(window_report[17]))eventClose(&retry);
        nwindowClose(&window);
        window_report[19]=nwindowIsValid(&window);window_report[20]=window.is_connected;
        window_report[21]=eventActive(&window.event);
        window_report[22]=viCloseLayer(&layer);window_report[23]=layer.initialized;
    }
    vi_report[14]=viCloseDisplay(&a);vi_report[15]=a.initialized;
    vi_report[16]=viCloseDisplay(&b);
    vi_report[17]=viCloseDisplay(&b);
    viExit();
    vi_report[18]=viGetSession_IApplicationDisplayService()->session;
    vi_report[19]=viGetSession_IHOSBinderDriverRelay()->session;
    vi_report[0]=1;
    return result;
}
