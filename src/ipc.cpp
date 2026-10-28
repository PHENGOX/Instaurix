#include "ipc.hpp"
#include <cstddef>
#include "bus.hpp"
#include "memory.hpp"

namespace Ipc {
uint32_t parseControl(const Buffer& request,ControlRequest& plan)
{
    auto read = [&](size_t at) {
        uint32_t value = 0;
        for (size_t i=0;i<4;++i) value |= uint32_t(request[at+i]) << (8*i);
        return value;
    };
    const auto type = read(0), header = read(4);
    ControlRequest staged;
    if (type == 2)
    {
        if (header) return InvalidMessage;
        staged.close = true;
    }
    else
    {
        if (type != 5 || (header & ~0x3FFU)) return UnsupportedFormat;
        if (header < 8 || header > (BufferSize-8)/4) return InvalidMessage;
        if (read(16) != 0x49434653) return InvalidMessage;
        if (read(20) || read(28)) return UnsupportedFormat;
        if(header==9 && read(24)==4){staged.clone=true;staged.taggedClone=true;staged.cloneTag=read(32);}
        else if (header != 8) staged.result = InvalidArguments;
        else if (read(24) == 0) staged.convert = true;
        else if (read(24) == 2) staged.clone = true;
        else if (read(24) == 3) staged.queryPointerSize = true;
        else staged.result = UnknownCommand;
    }
    plan = staged;
    return 0;
}

uint32_t parseManager(const Buffer& request, bool initialized, ManagerRequest& plan)
{
    auto read = [&](size_t offset,size_t count) {
        uint64_t value = 0;
        for (size_t i=0;i<count;++i) value |= uint64_t(request[offset+i]) << (8*i);
        return value;
    };
    if (read(0,4) != 4) return UnsupportedFormat;
    const auto header = read(4,4);
    if (header & ~uint64_t(0x800003FF)) return UnsupportedFormat;
    const bool pid = (header & 0x80000000) != 0;
    // Accept only the SendPid marker; this local profile ignores its PID bytes.
    if (pid && read(8,4) != 1) return UnsupportedFormat;
    const size_t raw = pid ? 20 : 8, cmif = (raw+15)&~size_t(15);
    const auto words = header & 0x3FF;
    if (words < 8 || words > (BufferSize-raw)/4) return InvalidMessage;
    if (read(cmif,4) != 0x49434653) return InvalidMessage;
    if (read(cmif+4,4) || read(cmif+12,4)) return UnsupportedFormat;
    ManagerRequest staged;
    const auto command = read(cmif+8,4), payloadSize = words*4-32;
    if (pid && command != 0) return UnsupportedFormat;
    if (command == 0)
    {
        if (!pid || payloadSize != 8) staged.result = InvalidArguments;
        else if (initialized) staged.result = AlreadyInitialized;
        else staged.initialize = true;
        // Guest PID and placeholder bytes are never trusted as an identity.
    }
    else if (command == 1)
    {
        if (payloadSize != 8) staged.result = InvalidArguments;
        else if (!initialized) staged.result = NotInitialized;
        else
        {
            constexpr char name[] = "emu:demo";
            constexpr uint8_t fileName[8]{'e','m','u',':','f','s',0,0};
            constexpr char displayName[]="emu:disp";
            constexpr uint8_t inputName[8]{'e','m','u',':','p','a','d',0};
            constexpr uint8_t apmName[8]{'a','p','m',0,0,0,0,0};
            constexpr char appletName[]="appletOE";
            constexpr uint8_t hidName[8]{'h','i','d',0,0,0,0,0};
            constexpr uint8_t timeName[8]{'t','i','m','e',':','u',0,0};
            constexpr uint8_t fspName[8]{'f','s','p','-','s','r','v',0};
            constexpr uint8_t viName[8]{'v','i',':','u',0,0,0,0};
            constexpr uint8_t nvName[8]{'n','v','d','r','v',0,0,0};
            bool matches = true, fileMatches = true, displayMatches = true, inputMatches = true, apmMatches=true, appletMatches=true, hidMatches=true, timeMatches=true, fspMatches=true, viMatches=true, nvMatches=true;
            for (size_t i=0;i<8;++i) {
                matches &= request[cmif+16+i] == uint8_t(name[i]);
                fileMatches &= request[cmif+16+i] == fileName[i];
                displayMatches &= request[cmif+16+i] == uint8_t(displayName[i]);
                inputMatches &= request[cmif+16+i] == inputName[i];
                apmMatches &= request[cmif+16+i] == apmName[i];
                appletMatches &= request[cmif+16+i] == uint8_t(appletName[i]);
                hidMatches &= request[cmif+16+i] == hidName[i];
                timeMatches &= request[cmif+16+i] == timeName[i];
                fspMatches &= request[cmif+16+i] == fspName[i];
                viMatches &= request[cmif+16+i] == viName[i];
                nvMatches &= request[cmif+16+i] == nvName[i];
            }
            if (matches) staged.openDemo = true;
            else if (fileMatches) staged.openFiles = true;
            else if (displayMatches) staged.openDisplay = true;
            else if (inputMatches) staged.openInput = true;
            else if (apmMatches) staged.openApm = true;
            else if (appletMatches) staged.openApplet = true;
            else if (hidMatches) staged.openHid = true;
            else if (timeMatches) staged.openTime = true;
            else if (fspMatches) staged.openFsp = true;
            else if (viMatches) staged.openVi = true;
            else if (nvMatches) staged.openNv = true;
            else staged.result = PortNotFound;
        }
    }
    else staged.result = UnknownCommand;
    plan = staged;
    return 0;
}

uint32_t parseApm(const Buffer& request,bool session,ApmRequest& plan)
{
    Layout layout;
    const auto status=parseBuffers(request,nullptr,0,layout);
    if(status) return status;
    ApmRequest staged;
    if(session || layout.command!=0) staged.result=UnknownCommand;
    else if(layout.payloadSize || layout.sends || layout.receives) staged.result=InvalidArguments;
    else staged.openSession=true;
    plan=staged;return 0;
}

void managerResponse(Buffer& response,uint32_t result,uint32_t movedHandle)
{
    Buffer staged{};
    auto write = [&](size_t at,uint64_t value,size_t count=4) {
        for (size_t i=0;i<count;++i) staged[at+i] = uint8_t(value >> (8*i));
    };
    write(4,8 | (movedHandle ? 0x80000000U : 0));
    if (movedHandle) { write(8,1U<<5); write(12,movedHandle); }
    write(16,0x4F434653); write(24,result);
    response = staged;
}

void valueResponse(Buffer& response,uint32_t result,uint64_t value)
{
    managerResponse(response,result);
    if (!result) {
        response[4]=10;
        for(size_t i=0;i<8;++i) response[32+i]=uint8_t(value>>(8*i));
    }
}

void copiedHandleResponse(Buffer& response,uint32_t result,uint32_t handle)
{
    managerResponse(response,result,result ? 0 : handle);
    if(!result && handle) response[8]=2;
}

uint32_t parseHid(const Buffer& request,bool resource,uint64_t appletId,HidRequest& plan)
{
    auto read=[&](size_t at,size_t n=4) { uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(request[at+i])<<(8*i);return v; };
    if(read(0)!=4 || (read(4)&~uint64_t(0x800003FF))) return UnsupportedFormat;
    const bool pid=read(4)&0x80000000U;
    if(pid && read(8)!=1) return UnsupportedFormat;
    const size_t raw=pid?20:8,cmif=pid?32:16;
    const auto words=read(4)&0x3FF;
    if(words<8 || words>(BufferSize-raw)/4) return InvalidMessage;
    if(read(cmif)!=0x49434653) return InvalidMessage;
    if(read(cmif+4) || read(cmif+12)) return UnsupportedFormat;
    HidRequest next;
    if(read(cmif+8)!=0) next.result=UnknownCommand;
    else if(resource) {
        if(pid || words!=8) next.result=InvalidArguments;
        else next.getMemory=true;
    } else {
        if(!pid || words!=10 || !appletId || read(cmif+16,8)!=appletId) next.result=InvalidArguments;
        else next.createResource=true;
    }
    plan=next;return 0;
}

uint32_t dispatchInput(const Buffer& request,Buffer& response,Bus& bus,uint64_t ipcAddress,uint32_t buttons)
{
    Layout layout;
    const auto status=parseBuffers(request,&bus,ipcAddress,layout);
    if(status) return status;
    const uint32_t result=layout.command ? UnknownCommand :
        (layout.payloadSize || layout.sends || layout.receives) ? InvalidArguments : 0;
    valueResponse(response,result,buttons);
    return 0;
}

uint32_t parseBuffers(const Buffer& request,Bus* bus,uint64_t ipcAddress,Layout& layout)
{
    auto read = [&](size_t offset, size_t count) {
        uint64_t value = 0;
        for (size_t i=0;i<count;++i) value |= uint64_t(request[offset+i]) << (8*i);
        return value;
    };
    const auto header0 = read(0,4), header1 = read(4,4);
    const auto sends = (header0 >> 20) & 15, receives = (header0 >> 24) & 15;
    if ((header0 & 0xF00FFFFF) != 4 || sends > 1 || receives > 1 ||
        (header1 & ~uint64_t(0x3FF))) return UnsupportedFormat;
    const auto words = header1 & 0x3FF;
    const size_t raw = 8 + 12*(sends+receives), cmif = (raw+15) & ~size_t(15);
    // Raw data includes 16 bytes total CMIF alignment padding and its 16-byte header.
    if (words < 8 || words > (BufferSize-raw)/4) return InvalidMessage;
    if (read(cmif,4) != 0x49434653) return InvalidMessage; // SFCI
    if (read(cmif+4,4) != 0 || read(cmif+12,4) != 0) return UnsupportedFormat;
    const auto command = read(cmif+8,4), payloadSize = words*4-32;
    uint64_t source = 0, sourceSize = 0, destination = 0, destinationSize = 0;
    for (size_t i=0;i<sends+receives;++i)
    {
        const size_t offset = 8+i*12;
        const auto flags = read(offset+8,4);
        if ((flags & 3) != 0) return UnsupportedFormat;
        const uint64_t size = read(offset,4) | (((flags >> 24) & 15) << 32);
        const uint64_t address = read(offset+4,4) | (((flags >> 28) & 15) << 32) |
            (((flags >> 2) & 0x3FFFFF) << 36);
        if (size > MaxTransfer) return TransferLimit;
        if (!bus) return UnsupportedFormat;
        const auto permission = i < sends ? Memory::Read : Memory::Write;
        if (size && (!bus->isRamRange(address,size) || !bus->getMemory().canAccess(address,size,permission)))
            return InvalidBuffer;
        if (i < sends) { source = address; sourceSize = size; }
        else
        {
            destination = address; destinationSize = size;
            if (size && (address >= ipcAddress ? address-ipcAddress < BufferSize : ipcAddress-address < size))
                return InvalidBuffer;
        }
    }
    layout = {uint32_t(command),cmif+16,size_t(payloadSize),size_t(sends),size_t(receives),
        source,sourceSize,destination,destinationSize};
    return 0;
}

uint32_t dispatchDemo(const Buffer& request, Buffer& response, uint64_t virtualNanoseconds,
    Bus* bus, uint64_t ipcAddress, Transfer* transfer)
{
    Layout layout;
    const auto status = parseBuffers(request,bus,ipcAddress,layout);
    if(status) return status;
    const auto [command,payload,payloadSize,sends,receives,source,sourceSize,destination,destinationSize] = layout;
    if((sends || receives) && !transfer) return UnsupportedFormat;
    const auto cmif=payload-16;
    auto read = [&](size_t offset,size_t count) {
        uint64_t value=0;
        for(size_t i=0;i<count;++i) value|=uint64_t(request[offset+i])<<(8*i);
        return value;
    };
    uint32_t serviceResult = 0;
    uint64_t value = 0;
    bool hasValue = false;
    Transfer stagedTransfer;
    if ((command == 0 || command == 1) && (sends || receives)) return UnsupportedFormat;
    if (command == 0)
    {
        if (payloadSize != 16) serviceResult = InvalidArguments;
        else { value = read(cmif+16,8) + read(cmif+24,8); hasValue = true; }
    }
    else if (command == 1)
    {
        if (payloadSize != 0) serviceResult = InvalidArguments;
        else { value = virtualNanoseconds; hasValue = true; }
    }
    else if (command == 2)
    {
        if (payloadSize || sends != 1 || receives != 1 || destinationSize < sourceSize)
            serviceResult = InvalidArguments;
        else
        {
            stagedTransfer.address = destination;
            stagedTransfer.bytes.resize(static_cast<size_t>(sourceSize));
            for (size_t i=0;i<stagedTransfer.bytes.size();++i) stagedTransfer.bytes[i] = bus->getMemory().read8(source+i);
            value = sourceSize; hasValue = true;
        }
    }
    else serviceResult = UnknownCommand;
    Buffer staged{};
    auto write = [&](size_t offset,uint64_t data,size_t count) {
        for (size_t i=0;i<count;++i) staged[offset+i] = uint8_t(data >> (8*i));
    };
    write(4,hasValue ? 10 : 8,4); // HIPC response type 0, no descriptors.
    write(16,0x4F434653,4); // SFCO
    write(24,serviceResult,4);
    if (hasValue) write(32,value,8);
    response = staged;
    if (transfer) *transfer = std::move(stagedTransfer);
    return 0;
}
}
