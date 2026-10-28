#include "gpu_packets.hpp"
#include <new>
#include <utility>
namespace Gpu {
DecodeResult decodePushbuffer(const SubmissionSnapshot& input,DecodedPushbuffer& output){
    using R=DecodeResult;size_t total=0;
    if(input.buffers.empty())return R::Malformed;
    for(const auto& buffer:input.buffers){if(buffer.bytes.empty() || buffer.bytes.size()%4)return R::Malformed;if(buffer.bytes.size()>0x10000-total)return R::TransferLimit;total+=buffer.bytes.size();}
    try {
        DecodedPushbuffer staged;
        for(size_t segment=0;segment<input.buffers.size();++segment){
            const auto& bytes=input.buffers[segment].bytes;const auto words=bytes.size()/4;
            auto read=[&](size_t index){uint32_t value=0;for(size_t i=0;i<4;++i)value|=uint32_t(bytes[index*4+i])<<(8*i);return value;};
            for(size_t position=0;position<words;){
                const auto origin=position;const uint32_t header=read(position++);
                if(!header){++staged.dmaNops;continue;}
                const uint32_t opcode=header>>29,method=header&0xFFF,subchannel=(header>>13)&7,count=(header>>16)&0x1FFF;
                if(opcode!=1 && opcode!=3 && opcode!=4 && opcode!=5)return R::Unsupported;
                if(header&(1U<<12))return R::Malformed;
                if(opcode==4){staged.methods.push_back({segment,origin,subchannel,method*4,count});continue;}
                // The initial reference profile requires each packet to fit one GPFIFO range.
                if(!count || count>words-position)return R::Malformed;
                if((opcode==1 && count-1>0xFFF-method) || (opcode==5 && count>1 && method==0xFFF))return R::Malformed;
                for(uint32_t i=0;i<count;++i){
                    const auto address=method+(opcode==1?i:opcode==5 && i?1U:0U);
                    staged.methods.push_back({segment,origin,subchannel,address*4,read(position++)});
                }
            }
        }
        output=std::move(staged);return R::Success;
    }catch(const std::bad_alloc&){return R::OutOfResource;}
}
}
