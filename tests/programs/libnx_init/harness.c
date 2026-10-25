/* Minimal no-service application. Weak app hooks are intentional app policy,
 * not replacements for the libnx initialization functions being tested. */
#include "internal.h"
#include "runtime/env.h"
#include <reent.h>
#ifdef REFERENCE_SM
#include "services/sm.h"
#include "runtime/diag.h"
#endif
#ifdef REFERENCE_DEFAULT_APP
#include "services/sm.h"
#include "services/apm.h"
#include "runtime/hosversion.h"
#endif
extern void __libnx_exit(int code) __attribute__((noreturn));
extern void __nx_exit(unsigned result,void* retaddr) __attribute__((noreturn));
#if defined(REFERENCE_SM) || defined(REFERENCE_DEFAULT_APP)
volatile u64 report[32] __attribute__((section(".data.report")))={0};
#else
volatile u64 report[16] __attribute__((section(".data.report")))={0};
#endif
char* fake_heap_start;
char* fake_heap_end;
#ifdef REFERENCE_DEFAULT_APP
extern u32 __nx_applet_type;
#else
u32 __nx_applet_type=0;
#endif
extern u64 __tls_initial_value;
extern u64 __tls_initial_end;
static void __attribute__((constructor)) constructor_probe(void) { report[0]|=2; }
#ifndef REFERENCE_DEFAULT_APP
void __appInit(void) {
#ifdef REFERENCE_SM
    report[16]=smInitialize();
    if(report[16]) diagAbortWithResult((unsigned)report[16]);
    report[17]=smInitialize();
    if(report[17]) diagAbortWithResult((unsigned)report[17]);
    report[18]=smGetServiceSession()->session;
    report[19]=smGetServiceSession()->pointer_buffer_size;
#endif
    report[0]|=1;
}
void __appExit(void) {
#ifdef REFERENCE_SM
    smExit();report[27]=smGetServiceSession()->session;
#endif
    report[0]|=8;
}
#endif
int main(int argc,char** argv)
{
    ThreadVars* tv=getThreadVars();
    report[0]|=4;report[1]=(u64)armGetTls();report[2]=tv->magic;report[3]=tv->handle;
    report[4]=(u64)tv->reent;report[5]=(u64)tv->thread_ptr;report[6]=(u64)tv->tls_tp;
    report[7]=(u64)fake_heap_start;report[8]=(u64)fake_heap_end;report[9]=*(u64*)__tls_start;
    report[10]=argc;report[11]=(u64)argv;report[12]=envIsNso();
    report[13]=tv->thread_ptr ? tv->thread_ptr->stack_sz : 0;
    if(tv->magic!=THREADVARS_MAGIC || tv->handle!=envGetMainThreadHandle() || tv->reent!=_impure_ptr ||
       !tv->thread_ptr || tv->tls_tp!=__tls_start-getTlsStartOffset() ||
       fake_heap_start!=envGetHeapOverrideAddr() || fake_heap_end-fake_heap_start!=envGetHeapOverrideSize() ||
       *(u64*)__tls_start!=0x1122334455667788ULL || argc || envIsNso() || !argv || argv[0]) return 1;
#ifdef REFERENCE_SM
    Service unavailable={0}, clone={0};
    report[20]=smGetService(&unavailable,"set:sys");report[21]=unavailable.session;
    report[22]=serviceClone(smGetServiceSession(),&clone);report[23]=clone.session;
    if(report[22]) return 2;
    Handle moved=0;
    const SmServiceName name=smEncodeName("set:sys");
    report[24]=serviceDispatchIn(&clone,1,name,.out_handle_attrs={SfOutHandleAttr_HipcMove},.out_handles=&moved);
    report[28]=moved;
    serviceClose(&clone);report[29]=clone.session;
    smExit();report[25]=smGetServiceSession()->session;
    report[26]=smGetService(&unavailable,"set:sys");
    if(!report[20] || report[21] || report[24]!=report[20] || report[26]!=report[20] ||
       report[18]==report[23] || !report[25] || report[28] || report[29]) return 3;
#endif
    return 77;
}
void exit(int code) { report[14]=code;__libnx_exit(code); }
void diagAbortWithResult(unsigned result) {
    report[15]=result;
#ifdef REFERENCE_DEFAULT_APP
    report[16]=hosversionGet();report[17]=__nx_applet_type;report[18]=getThreadVars()->magic;
    report[19]=smGetServiceSession()->session;
    report[20]=apmGetServiceSession()->session;report[21]=apmGetServiceSession_Session()->session;
#endif
    __nx_exit(result,envGetExitFuncPtr());
}
/* No extra services or arguments are allowed to disappear behind success stubs. */
void missing_dependency(unsigned marker) { report[15]=marker;__nx_exit(marker,envGetExitFuncPtr()); }
