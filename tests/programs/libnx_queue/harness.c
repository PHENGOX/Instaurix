#define main referenceViBaseMain
#include "../libnx_fsdev/harness.c"
#undef main
#include "services/vi.h"
#include "display/binder.h"
#include "display/buffer_producer.h"
volatile u64 queue_report[16] __attribute__((section(".data.queue_report")))={0};

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
        Binder producer;Event event;
        vi_report[20]=(u32)layer.igbp_binder_obj_id;
        binderCreate(&producer,layer.igbp_binder_obj_id);
        vi_report[21]=binderInitSession(&producer,viGetSession_IHOSBinderDriverRelay());
        vi_report[22]=producer.initialized;
        vi_report[23]=binderGetNativeHandle(&producer,0xF,&event);
        BqBufferOutput output={0};
        queue_report[0]=bqConnect(&producer,2,false,&output);
        queue_report[1]=output.width;queue_report[2]=output.height;
        queue_report[3]=output.transformHint;queue_report[4]=output.numPendingBuffers;
        queue_report[5]=bqConnect(&producer,2,false,&output);
        s32 query=-123;
        queue_report[6]=bqQuery(&producer,0,&query);queue_report[7]=(u64)(s64)query;
        queue_report[8]=bqDisconnect(&producer,2);
        queue_report[9]=bqDisconnect(&producer,2);
        queue_report[10]=bqConnect(&producer,2,false,&output);
        queue_report[11]=bqDisconnect(&producer,2);
        binderClose(&producer);
        vi_report[25]=producer.created;vi_report[26]=producer.initialized;
        vi_report[27]=viCloseLayer(&layer);
        vi_report[28]=layer.initialized;vi_report[29]=layer.layer_id;
        vi_report[30]=viCloseLayer(&layer);
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
