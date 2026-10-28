#define main referenceViBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/vi.h"
#include "display/native_window.h"
#include "kernel/svc.h"
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
