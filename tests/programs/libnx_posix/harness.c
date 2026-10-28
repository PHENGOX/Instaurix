#define main referenceStartupMain
#include "../libnx_fsdev/harness.c"
#undef main
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

volatile u64 posix_report[32] __attribute__((section(".data.posix_report")))={0};
static char first[64],second[64];
static int same(const char* a,const char* b,unsigned size) {
    for(unsigned i=0;i<size;++i)if(a[i]!=b[i])return 0;
    return 1;
}
int main(int argc,char** argv) {
    const int result=referenceStartupMain(argc,argv);
    posix_report[0]=99;
    int a=open("sdmc:/sample.txt",O_RDONLY,0);
    posix_report[1]=(u64)(s64)a;posix_report[2]=errno;
    if(a<0){posix_report[0]=2;return result;}
    int b=open("sdmc:/sample.txt",O_RDONLY,0);
    posix_report[3]=(u64)(s64)b;
    if(b<0){close(a);return result;}
    posix_report[4]=(u64)(s64)read(a,first,7);
    posix_report[5]=(u64)(s64)read(a,first+7,57);
    posix_report[6]=(u64)(s64)read(a,first+30,1);
    posix_report[7]=(u64)(s64)read(b,second,64);
    posix_report[8]=same(first,"Switch2Emu read-only fixture.\n",30);
    posix_report[9]=same(second,first,30);
    posix_report[10]=(u64)(s64)read(a,first,0);
    posix_report[11]=(u64)(s64)close(a);
    errno=0;posix_report[12]=(u64)(s64)read(a,first,1);posix_report[13]=errno;
    int c=open("sdmc:/sample.txt",O_RDONLY,0);posix_report[14]=(u64)(s64)c;
    posix_report[15]=(u64)(s64)read(c,first,3);posix_report[16]=same(first,"Swi",3);
    posix_report[17]=(u64)(s64)close(c);posix_report[18]=(u64)(s64)close(b);
    errno=0;posix_report[19]=(u64)(s64)close(a);posix_report[20]=errno;
    errno=0;int absent=open("sdmc:/missing.txt",O_RDONLY,0);posix_report[21]=(u64)(s64)absent;posix_report[22]=errno;if(absent>=0)close(absent);
    errno=0;int writefd=open("sdmc:/sample.txt",O_WRONLY,0);posix_report[23]=(u64)(s64)writefd;posix_report[24]=errno;if(writefd>=0)close(writefd);
    errno=0;int invalid=open("sdmc:/sample.txt",O_RDONLY|O_APPEND,0);posix_report[25]=(u64)(s64)invalid;posix_report[26]=errno;if(invalid>=0)close(invalid);
    posix_report[0]=1;
    return result;
}
