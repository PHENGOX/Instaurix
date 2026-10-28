#include "ipc_binder.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include <algorithm>
#include <string_view>
namespace Ipc {
uint32_t parseBinderTransaction(const Buffer& request,Bus& bus,uint64_t ipcAddress,BinderTransaction& plan){
    auto read=[&](size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(request[at+i])<<(8*i);return v;};
    const bool automatic=read(0)==0x01110004;
    size_t cmif=32,descriptor=8;
    if(automatic){
        // Zero pointer-buffer capacity: null X/C fallbacks, active normal A/B buffers.
        if(read(4)!=0xC0C || read(8) || read(12) || read(84,2) || read(88,8))return UnsupportedFormat;
        cmif=48;descriptor=16;
    }else if(read(0)!=0x01100004 || read(4)!=11)return UnsupportedFormat;
    if(read(cmif)!=0x49434653)return InvalidMessage;
    if(read(cmif+4) || read(cmif+12) || read(cmif+8)!=(automatic?3:0))return UnsupportedFormat;
    BinderTransaction next;
    next.object=uint32_t(read(cmif+16));next.code=uint32_t(read(cmif+20));next.flags=uint32_t(read(cmif+24));
    for(size_t i=0;i<2;++i){
        const auto at=descriptor+i*12,flags=read(at+8);
        if(flags&3)return UnsupportedFormat;
        const auto size=read(at)|((flags>>24&15)<<32);
        const auto address=read(at+4)|((flags>>28&15)<<32)|((flags>>2&0x3FFFFF)<<36);
        if(size>MaxTransfer)return TransferLimit;
        if(!size || !bus.isRamRange(address,size) || !bus.getMemory().canAccess(address,size,i?Memory::Write:Memory::Read))return InvalidBuffer;
        if(address>=ipcAddress ? address-ipcAddress<BufferSize : ipcAddress-address<size)return InvalidBuffer;
        if(!i){next.source=address;next.sourceSize=size;}else{next.destination=address;next.destinationSize=size;}
    }
    if(next.source>=next.destination ? next.source-next.destination<next.destinationSize : next.destination-next.source<next.sourceSize)return InvalidBuffer;
    plan=next;return 0;
}
uint32_t parseQueueRegistration(Bus& bus,const BinderTransaction& plan,QueueRegistration& registration,Transfer& reply){
    if(plan.code!=14 || plan.flags || plan.sourceSize<104 || plan.sourceSize>1024 || plan.destinationSize!=1024)return InvalidArguments;
    std::vector<uint8_t> bytes(size_t(plan.sourceSize));for(size_t i=0;i<bytes.size();++i)bytes[i]=bus.getMemory().read8(plan.source+i);
    auto read=[&](size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(bytes[at+i])<<(8*i);return v;};
    constexpr std::string_view token="android.gui.IGraphicBufferProducer";
    if(read(0)!=bytes.size()-16 || read(4)!=16 || read(8) || read(12)!=bytes.size() || read(16)!=0x100 || read(20)!=token.size())return InvalidArguments;
    for(size_t i=0;i<=token.size();++i)if(bytes[24+2*i]!=(i<token.size()?uint8_t(token[i]):0) || bytes[25+2*i])return InvalidArguments;
    for(size_t i=24+2*(token.size()+1);i<96;++i)if(bytes[i])return InvalidArguments;
    constexpr size_t args=96,flat=112; // Pinned token followed by slot/presence and flattened header.
    QueueRegistration next;next.slot=read(args);next.present=read(args+4)==1;
    if(next.slot>=64 || read(args+4)>1)return InvalidArguments;
    if(!next.present){if(bytes.size()!=104)return InvalidArguments;}
    else {
        if(bytes.size()!=476 || read(args+8)!=364 || read(args+12))return InvalidArguments;
        next.object=read(flat+44);if(!next.object)return InvalidArguments;
        auto surface=std::make_shared<QueueSurface>();surface->object=next.object;
        surface->blockLinear=read(flat+40+17*4)==3;
        surface->width=read(flat+40+13*4);surface->height=read(flat+40+14*4);
        if(!surface->width || surface->width>256 || !surface->height || surface->height>256)return InvalidArguments;
        if(!surface->blockLinear && (surface->width!=16 || surface->height!=16))return InvalidArguments;
        surface->pitch=(surface->width*4+63)&~63U;
        surface->size=surface->blockLinear?surface->pitch*((surface->height+127)&~127U):4096;
        const std::array<uint32_t,10> outer{0x47424652,surface->width,surface->height,surface->pitch/4,1,0xB00,42,0,0,81};
        for(size_t i=0;i<outer.size();++i)if(read(flat+4*i)!=outer[i])return InvalidArguments;
        surface->offset=read(flat+40+20*4);
        if(surface->offset && (!surface->blockLinear || surface->offset%surface->size || surface->offset/surface->size>=3))return InvalidArguments;
        // Two bounded RGBA profiles. Unused planes and padding must remain zero.
        for(size_t i=0;i<81;++i){
            uint32_t expected=0;
            switch(i){case 0:expected=UINT32_MAX;break;case 1:expected=next.object;break;case 3:expected=0xDAFFCAFF;break;
            case 4:expected=42;break;case 6:expected=0xB00;break;case 7:case 8:case 16:case 17:expected=1;break;
            case 9:case 13:case 14:expected=16;break;case 10:case 27:expected=4096;break;case 11:expected=1;break;
            case 15:expected=0x00532120;break;case 18:expected=64;break;default:break;}
            if(i==10 || i==27)expected=surface->size;
            if(i==9)expected=surface->pitch/4;
            if(i==13)expected=surface->width;
            if(i==14)expected=surface->height;
            if(i==18)expected=surface->pitch;
            if(i==17)expected=surface->blockLinear?3:1;
            if(i==20)expected=surface->offset;
            if(i==21)expected=surface->blockLinear?0xfe:0;
            if(i==22)expected=surface->blockLinear?4:0;
            if(read(flat+40+4*i)!=expected)return InvalidArguments;
        }
        std::copy(bytes.begin()+flat,bytes.begin()+flat+364,surface->descriptor.begin());
        next.surface=std::move(surface);
    }
    Transfer staged;staged.address=plan.destination;staged.bytes.resize(16);staged.bytes[4]=16;staged.bytes[12]=16;
    registration=next;reply=std::move(staged);return 0;
}
uint32_t buildQueueAccess(Bus& bus,const BinderTransaction& plan,const std::array<uint32_t,64>& slots,uint64_t acquired,Transfer& reply,uint64_t& nextAcquired,const QueueSurfaces& surfaces,uint32_t start){
    if(plan.flags || plan.sourceSize<100 || plan.sourceSize>1024 || plan.destinationSize!=1024)return InvalidArguments;
    std::vector<uint8_t> bytes(size_t(plan.sourceSize));for(size_t i=0;i<bytes.size();++i)bytes[i]=bus.getMemory().read8(plan.source+i);
    auto read=[&](size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(bytes[at+i])<<(8*i);return v;};
    constexpr std::string_view token="android.gui.IGraphicBufferProducer";
    if(read(0)!=bytes.size()-16 || read(4)!=16 || read(8) || read(12)!=bytes.size() || read(16)!=0x100 || read(20)!=token.size())return InvalidArguments;
    for(size_t i=0;i<=token.size();++i)if(bytes[24+2*i]!=(i<token.size()?uint8_t(token[i]):0) || bytes[25+2*i])return InvalidArguments;
    for(size_t i=24+2*(token.size()+1);i<96;++i)if(bytes[i])return InvalidArguments;
    uint64_t next=acquired;uint32_t slot=read(96);size_t replySize=16;
    if(plan.code==3){
        if(bytes.size()!=116 || slot>1 || read(108)!=1 || read(112)!=0xB00)return InvalidArguments;
        const QueueSurface* geometry=nullptr;for(const auto& surface:surfaces)if(surface){geometry=surface.get();break;}
        if(read(100)!=(geometry?geometry->width:16) || read(104)!=(geometry?geometry->height:16))return InvalidArguments;
        const auto first=geometry && geometry->blockLinear?start%64:0;
        slot=64;for(uint32_t i=0;i<64;++i){const auto candidate=(first+i)%64;if(slots[candidate] && !(acquired&(uint64_t(1)<<candidate))){slot=candidate;break;}}
        if(slot<64)next|=uint64_t(1)<<slot;
        replySize=72;
    }else if(plan.code==1){
        if(bytes.size()!=100 || slot>=64)return InvalidArguments;
        if(!slots[slot] || !(acquired&(uint64_t(1)<<slot)))return NotInitialized;
        replySize=396;
    }else if(plan.code==7){
        if(bytes.size()!=192 || slot>=64 || read(100)!=84 || read(104))return InvalidArguments;
        for(size_t at=108;at<192;at+=4)if(read(at)!=(at==152?1U:0U))return InvalidArguments;
        if(!slots[slot] || !(acquired&(uint64_t(1)<<slot)))return NotInitialized;
        next&=~(uint64_t(1)<<slot);replySize=36; // Immediate local CPU consumer, no GPU backlog.
    }else if(plan.code==8){
        if(bytes.size()!=144 || slot>=64 || read(100)!=36 || read(104))return InvalidArguments;
        for(size_t at=108;at<144;at+=4)if(read(at))return InvalidArguments; // Only an empty CPU-only fence.
        if(!slots[slot] || !(acquired&(uint64_t(1)<<slot)))return NotInitialized;
        next&=~(uint64_t(1)<<slot);
    }else return UnknownCommand;
    Transfer staged;staged.address=plan.destination;staged.bytes.resize(replySize);
    auto put=[&](size_t at,uint32_t value){for(size_t i=0;i<4;++i)staged.bytes[at+i]=uint8_t(value>>(8*i));};
    put(0,uint32_t(replySize-16));put(4,16);put(12,uint32_t(replySize));
    if(plan.code==3){put(16,slot<64?slot:UINT32_MAX);put(20,1);put(24,36);put(68,slot<64?0:uint32_t(-11));}
    if(plan.code==7){put(16,surfaces[slot]->width);put(20,surfaces[slot]->height);}
    if(plan.code==1){
        if(!surfaces[slot])return NotInitialized;
        put(16,1);put(20,364);
        std::copy(surfaces[slot]->descriptor.begin(),surfaces[slot]->descriptor.end(),staged.bytes.begin()+28);
    }
    reply=std::move(staged);nextAcquired=next;return 0;
}
uint32_t buildQueueReply(Bus& bus,const BinderTransaction& plan,bool connected,Transfer& transfer,bool& nextConnected){
    if(plan.flags || plan.sourceSize<16 || plan.sourceSize>1024 || plan.destinationSize!=1024)return InvalidArguments;
    std::vector<uint8_t> bytes(size_t(plan.sourceSize));
    for(size_t i=0;i<bytes.size();++i)bytes[i]=bus.getMemory().read8(plan.source+i);
    auto read=[&](size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(bytes[at+i])<<(8*i);return v;};
    const auto size=read(0),offset=read(4),objects=read(8),objectsOffset=read(12);
    if(offset!=16 || size!=bytes.size()-16 || objects || objectsOffset!=bytes.size())return InvalidArguments;
    constexpr std::string_view token="android.gui.IGraphicBufferProducer";
    const size_t tokenEnd=8+((2*(token.size()+1)+3)&~size_t(3));
    if(size<tokenEnd || read(16)!=0x100 || read(20)!=token.size())return InvalidArguments;
    for(size_t i=0;i<=token.size();++i)if(read(24+2*i,2)!=(i<token.size()?uint8_t(token[i]):0))return InvalidArguments;
    const auto args=16+tokenEnd;
    constexpr uint32_t width=1280,height=720,format=1; // Local RGBA_8888 profile.
    uint32_t queryValue=0;
    if(plan.code==10){
        if(size!=tokenEnd+12 || read(args) || read(args+4)!=2 || read(args+8))return InvalidArguments;
        if(connected)return AlreadyInitialized;
    }else if(plan.code==11){
        if(size!=tokenEnd+4 || read(args)!=2)return InvalidArguments;
        if(!connected)return NotInitialized;
    }else if(plan.code==9){
        if(size!=tokenEnd+4)return InvalidArguments;
        if(!connected)return NotInitialized;
        switch(read(args)){
        case 0:queryValue=width;break;
        case 1:queryValue=height;break;
        case 2:queryValue=format;break;
        default:return UnknownCommand;
        }
    }else return UnknownCommand;
    Transfer staged;staged.address=plan.destination;staged.bytes.resize(plan.code==10?36:plan.code==9?24:20);
    auto put=[&](size_t at,uint32_t v){for(size_t i=0;i<4;++i)staged.bytes[at+i]=uint8_t(v>>(8*i));};
    const auto payload=uint32_t(staged.bytes.size()-16);put(0,payload);put(4,16);put(12,uint32_t(staged.bytes.size()));
    // Explicit headless geometry/connectivity replies; no frame is produced here.
    if(plan.code==10){put(16,width);put(20,height);}
    if(plan.code==9)put(16,queryValue);
    nextConnected=plan.code==9?connected:plan.code==10;transfer=std::move(staged);return 0;
}
}
