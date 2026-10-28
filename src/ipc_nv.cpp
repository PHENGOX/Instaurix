#include "ipc_nv.hpp"
#include "bus.hpp"
#include "memory.hpp"
namespace Ipc {
uint32_t parseNvIoctl(const Buffer& packet,Bus& bus,uint64_t ipcAddress,NvIoctl& plan){
    auto read=[&](size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(packet[at+i])<<(8*i);return v;};
    size_t cmif=32,descriptors=8;const bool ioctl2=read(0)==0x01220004;
    if(ioctl2){
        if(read(4)!=0xC0B || read(8,8) || read(16,8)!=1 || read(100,2) || read(104,8))return UnsupportedFormat;
        cmif=64;descriptors=24;
    }else if(read(0)==0x01110004){
        if(read(4)!=0xC0B || read(8,8) || read(80,2) || read(84,8))return UnsupportedFormat;
        cmif=48;descriptors=16;
    }else if(read(0)!=0x01100004 || read(4)!=10)return UnsupportedFormat;
    if(read(cmif)!=0x49434653)return InvalidMessage;
    if(read(cmif+4) || read(cmif+8)!=(ioctl2?11:1) || read(cmif+12))return UnsupportedFormat;
    NvIoctl next;next.command=ioctl2?11:1;next.fd=uint32_t(read(cmif+16));next.request=uint32_t(read(cmif+20));
    const auto expected=(next.request>>16)&0x3FFF;
    const auto direction=next.request>>30;
    if((direction!=1 && direction!=3) || !expected)return UnsupportedFormat;
    for(size_t i=0;i<2;++i){
        const auto at=descriptors+12*(i && ioctl2?2:i);const auto flags=read(at+8);
        if(flags&3)return UnsupportedFormat;
        const auto size=read(at)|((flags>>24&15)<<32);
        const auto address=read(at+4)|((flags>>28&15)<<32)|((flags>>2&0x3FFFFF)<<36);
        if(i && direction==1){if(size || address || flags)return InvalidArguments;continue;}
        if(size>MaxTransfer)return TransferLimit;
        if(size!=expected)return InvalidArguments;
        if(!bus.isRamRange(address,size) || !bus.getMemory().canAccess(address,size,i?Memory::Write:Memory::Read))return InvalidBuffer;
        if(address>=ipcAddress?address-ipcAddress<BufferSize:ipcAddress-address<size)return InvalidBuffer;
        if(i)next.destination=address;else next.source=address;
    }
    if(direction==3 && next.source!=next.destination && (next.source>next.destination?next.source-next.destination<expected:next.destination-next.source<expected))return InvalidBuffer;
    if(ioctl2){
        const auto at=descriptors+12;const auto flags=read(at+8);
        if(flags&3)return UnsupportedFormat;
        const auto size=read(at)|((flags>>24&15)<<32);
        const auto address=read(at+4)|((flags>>28&15)<<32)|((flags>>2&0x3FFFFF)<<36);
        if(size>MaxTransfer)return TransferLimit;
        if(size){
            if(!bus.isRamRange(address,size) || !bus.getMemory().canAccess(address,size,Memory::Read))return InvalidBuffer;
            if(address>=ipcAddress?address-ipcAddress<BufferSize:ipcAddress-address<size)return InvalidBuffer;
            if(direction==3 && (address>=next.destination?address-next.destination<expected:next.destination-address<size))return InvalidBuffer;
        }
        next.extraSource=address;next.extraSize=size;
    }
    next.size=expected;plan=next;return 0;
}
}
