#include "gpu_submission.hpp"
#include <new>
namespace Gpu {
PreflightResult prepareKickoff(std::span<const uint8_t> parameters,std::span<const uint8_t> entries,const CommandReader& reader,SubmissionSnapshot& output){
    using R=PreflightResult;
    if(parameters.size()!=24 || !reader)return R::InvalidArguments;
    auto read=[](std::span<const uint8_t> bytes,size_t at,size_t count){uint64_t value=0;for(size_t i=0;i<count;++i)value|=uint64_t(bytes[at+i])<<(8*i);return value;};
    const auto count=read(parameters,8,4),flags=read(parameters,12,4);
    // The pointer at byte 0 is ignored by the client ABI; HIPC extra input is authoritative.
    if(!count || count>0x800 || entries.size()!=count*8 || flags!=4 || read(parameters,16,8))return R::InvalidArguments;
    try {
        SubmissionSnapshot staged;staged.flags=uint32_t(flags);staged.descriptors.reserve(size_t(count));staged.buffers.reserve(size_t(count));size_t total=0;
        for(size_t i=0;i<count;++i){
            const uint64_t descriptor=read(entries,i*8,8),address=descriptor&((1ULL<<40)-1),words=(descriptor>>42)&0x1FFFFF,bytes=words*4;
            if((descriptor&((3ULL<<40)|(1ULL<<63))) || !words || address%4)return R::InvalidArguments;
            if(address<AddressSpace::Base || address>=AddressSpace::Base+AddressSpace::Capacity || bytes>AddressSpace::Base+AddressSpace::Capacity-address)return R::InvalidAddress;
            if(bytes>0x10000-total)return R::TransferLimit;
            total+=size_t(bytes);staged.descriptors.push_back(descriptor);staged.buffers.push_back({VirtualAddress{address},std::vector<uint8_t>(size_t(bytes))});
        }
        for(auto& buffer:staged.buffers)if(!reader(buffer.address,buffer.bytes))return R::InvalidAddress;
        output=std::move(staged);return R::Success;
    }catch(const std::bad_alloc&){return R::OutOfResource;}
}
}
