#include "ipc.hpp"
#include "bus.hpp"
#include "memory.hpp"

namespace Ipc {
uint32_t dispatchDisplay(const Buffer& request,Buffer& response,Bus& bus,uint64_t ipcAddress,
    std::shared_ptr<const Frame>& frame)
{
    Layout layout;
    auto status=parseBuffers(request,&bus,ipcAddress,layout);
    if(status) return status;
    uint32_t result=0;
    std::shared_ptr<Frame> staged;
    auto read=[&](size_t at) { uint32_t v=0; for(size_t i=0;i<4;++i) v|=uint32_t(request[at+i])<<(8*i); return v; };
    if(layout.command!=0) result=UnknownCommand;
    else if(layout.sends!=1 || layout.receives || layout.payloadSize!=8) result=InvalidArguments;
    else {
        const auto width=read(layout.payload), height=read(layout.payload+4);
        const uint64_t size=uint64_t(width)*height*3;
        if(!width || !height || width>256 || height>256 || size>MaxTransfer || size!=layout.sourceSize) result=InvalidArguments;
        else {
            staged=std::make_shared<Frame>(); staged->width=width; staged->height=height;
            staged->rgb.resize(size);
            for(size_t i=0;i<size;++i) staged->rgb[i]=bus.getMemory().read8(layout.source+i);
        }
    }
    valueResponse(response,result,staged?staged->rgb.size():0);
    frame=std::move(staged);
    return 0;
}
}
