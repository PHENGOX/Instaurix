#include "ipc.hpp"
namespace Ipc {
uint32_t parseDomain(const Buffer& request,DomainRequest& plan,bool allowFileBuffers)
{
    auto read=[&](size_t at,size_t size=4) { uint64_t v=0;for(size_t i=0;i<size;++i) v|=uint64_t(request[at+i])<<(8*i);return v; };
    const auto header=read(0);
    if((header!=4 && (!allowFileBuffers || (header!=0x10004 && header!=0x1000004))) || (read(4)&~uint64_t(0x800003FF))) return UnsupportedFormat;
    DomainRequest next;
    next.sendPid=(read(4)&0x80000000U)!=0;
    size_t raw=8;
    if(header!=4) {
        if(next.sendPid)return UnsupportedFormat;
        if(header==0x10004) {
            if(read(8)&0x3F)return UnsupportedFormat;
            next.sendStatic=true;next.source=read(12)|((read(8)>>12&15)<<32)|((read(8)>>6&63)<<36);next.sourceSize=read(8)>>16;raw=16;
        }else {
            if((read(16)&3)!=1)return UnsupportedFormat;
            next.receiveBuffer=true;next.destination=read(12)|((read(16)>>28)<<32)|((read(16)>>2&0x3FFFFF)<<36);
            next.destinationSize=read(8)|((read(16)>>24&15)<<32);raw=20;
        }
    }
    const size_t domain=next.sendPid || next.receiveBuffer ? 32 : 16;
    // Support SendPid alone (fsp-srv) or SendPid plus one copied process handle (appletOE).
    if(next.sendPid) {
        if(read(8)==1) raw=20;
        else if(read(8)==3) { raw=24;next.copyHandlePresent=true;next.copyHandle=uint32_t(read(20)); }
        else return UnsupportedFormat;
    }
    const auto words=read(4)&0x3FF;
    if(words<8 || words>(BufferSize-raw)/4) return InvalidMessage;
    const auto type=read(domain,1), objects=read(domain+1,1), size=read(domain+2,2);
    next.object=uint32_t(read(domain+4));
    if(objects || read(domain+8) || read(domain+12)) return UnsupportedFormat;
    if(!next.object) return InvalidMessage;
    if(type==2) {
        if(next.sendPid || next.sendStatic || next.receiveBuffer || size || words!=8) return InvalidMessage;
        next.close=true;
    } else if(type==1) {
        if(size<16 || words!=(32+size+3)/4 || domain+16+size>raw+words*4) return InvalidMessage;
        const auto cmif=domain+16;
        if(read(cmif)!=0x49434653) return InvalidMessage;
        if(read(cmif+4) || read(cmif+12)) return UnsupportedFormat;
        next.command=uint32_t(read(cmif+8));next.payload=cmif+16;next.payloadSize=size-16;
    } else return UnsupportedFormat;
    plan=next;return 0;
}
void domainResponse(Buffer& response,uint32_t result,uint32_t object)
{
    Buffer next{};if(result) object=0;
    auto put=[&](size_t at,uint32_t value) { for(size_t i=0;i<4;++i) next[at+i]=uint8_t(value>>(8*i)); };
    put(4,object ? 13 : 12);put(16,object ? 1 : 0);
    put(32,0x4F434653);put(40,result);if(object) put(48,object);
    response=next;
}
void domainValueResponse(Buffer& response,uint32_t result,uint64_t value,size_t size)
{
    if(size!=1 && size!=4 && size!=8) { domainResponse(response,InvalidArguments);return; }
    domainResponse(response,result);if(result) return;
    response[4]=uint8_t(12+(size+3)/4);
    for(size_t i=0;i<size;++i) response[48+i]=uint8_t(value>>(8*i));
}
void domainEventResponse(Buffer& response,uint32_t result,uint32_t handle)
{
    domainResponse(response,result);if(result) return;
    response[7]=0x80;response[8]=2;
    for(size_t i=0;i<4;++i) response[12+i]=uint8_t(handle>>(8*i));
}
}
