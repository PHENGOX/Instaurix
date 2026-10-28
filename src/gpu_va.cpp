#include "gpu_va.hpp"
#include <algorithm>
namespace Gpu {
bool AddressSpace::valid(uint64_t address,uint64_t bytes){
    return bytes && bytes<=Capacity && address>=Base && address<Base+Capacity && bytes<=Base+Capacity-address;
}
size_t AddressSpace::count() const {return size_t(std::count_if(ranges.begin(),ranges.end(),[](const auto& range){return range.bytes!=0;}));}
uint64_t AddressSpace::reservedBytes() const {uint64_t total=0;for(const auto& range:ranges)total+=range.bytes;return total;}
AddressSpace::Result AddressSpace::reserve(uint64_t bytes,uint64_t alignment,std::optional<VirtualAddress> fixed,VirtualAddress& output){
    if(!bytes || bytes%PageSize || bytes>Capacity || alignment<PageSize || alignment>Capacity || (alignment&(alignment-1)))return Result::InvalidArguments;
    uint64_t candidate=fixed?fixed->value:Base;
    auto overlaps=[&](uint64_t start,const auto& range){return range.bytes && start<range.address.value+range.bytes && range.address.value<start+bytes;};
    if(fixed){
        if(candidate%alignment || !valid(candidate,bytes))return Result::InvalidArguments;
        if(std::any_of(ranges.begin(),ranges.end(),[&](const auto& range){return overlaps(candidate,range);}))return Result::Occupied;
    }else{
        for(;;){
            if(candidate>UINT64_MAX-(alignment-1))return Result::OutOfResource;
            candidate=(candidate+alignment-1)&~(alignment-1);
            if(!valid(candidate,bytes))return Result::OutOfResource;
            bool moved=false;for(const auto& range:ranges)if(overlaps(candidate,range)){candidate=range.address.value+range.bytes;moved=true;break;}
            if(!moved)break;
        }
    }
    const auto slot=std::find_if(ranges.begin(),ranges.end(),[](const auto& range){return !range.bytes;});
    if(slot==ranges.end())return Result::OutOfResource;
    *slot={VirtualAddress{candidate},bytes};output=VirtualAddress{candidate};return Result::Success;
}
AddressSpace::Result AddressSpace::release(VirtualAddress address,uint64_t bytes){
    if(address.value%PageSize || bytes%PageSize || !valid(address.value,bytes))return Result::InvalidArguments;
    const auto slot=std::find_if(ranges.begin(),ranges.end(),[&](const auto& range){return range.bytes && range.address.value==address.value;});
    if(slot==ranges.end())return Result::InvalidAddress;
    if(slot->bytes!=bytes)return Result::InvalidArguments;
    if(std::any_of(mappings.begin(),mappings.end(),[&](const auto& mapping){return mapping.bytes && mapping.address.value<address.value+bytes && address.value<mapping.address.value+mapping.bytes;}))return Result::Occupied;
    *slot={};return Result::Success;
}
size_t AddressSpace::mappingCount() const {return size_t(std::count_if(mappings.begin(),mappings.end(),[](const auto& mapping){return mapping.bytes!=0;}));}
bool AddressSpace::references(uint32_t object) const {return object && std::any_of(mappings.begin(),mappings.end(),[&](const auto& mapping){return mapping.object==object;});}
AddressSpace::Result AddressSpace::map(uint32_t object,uint64_t objectBytes,uint64_t offset,uint64_t bytes,std::optional<VirtualAddress> fixed,VirtualAddress& output){
    if(!object || !bytes || offset%PageSize || bytes%PageSize || offset>objectBytes || bytes>objectBytes-offset || bytes>Capacity)return Result::InvalidArguments;
    const auto slot=std::find_if(mappings.begin(),mappings.end(),[](const auto& mapping){return !mapping.bytes;});
    if(slot==mappings.end())return Result::OutOfResource;
    VirtualAddress address{};
    if(fixed){
        if(fixed->value%PageSize || !valid(fixed->value,bytes))return Result::InvalidArguments;
        const auto range=std::find_if(ranges.begin(),ranges.end(),[&](const auto& range){return range.bytes && range.address.value==fixed->value;});
        if(range==ranges.end())return Result::InvalidAddress;
        if(range->bytes!=bytes)return Result::InvalidArguments;
        if(std::any_of(mappings.begin(),mappings.end(),[&](const auto& mapping){return mapping.bytes && mapping.address.value==fixed->value;}))return Result::Occupied;
        address=*fixed;
    }else{
        const auto result=reserve(bytes,PageSize,{},address);if(result!=Result::Success)return result;
    }
    *slot={address,bytes,offset,object,!fixed.has_value()};output=address;return Result::Success;
}
AddressSpace::Result AddressSpace::unmap(VirtualAddress address){
    if(address.value%PageSize || !valid(address.value,PageSize))return Result::InvalidArguments;
    const auto slot=std::find_if(mappings.begin(),mappings.end(),[&](const auto& mapping){return mapping.bytes && mapping.address.value==address.value;});
    if(slot==mappings.end())return Result::InvalidAddress;
    if(slot->ownsReservation){
        const auto range=std::find_if(ranges.begin(),ranges.end(),[&](const auto& range){return range.bytes==slot->bytes && range.address.value==address.value;});
        if(range==ranges.end())return Result::InvalidAddress;
        *range={};
    }
    *slot={};return Result::Success;
}
bool AddressSpace::translate(VirtualAddress address,uint64_t bytes,Translation& output) const {
    if(!bytes)return false;
    for(const auto& mapping:mappings)if(mapping.bytes && address.value>=mapping.address.value){
        const auto delta=address.value-mapping.address.value;
        if(delta<mapping.bytes && bytes<=mapping.bytes-delta){output={mapping.object,mapping.offset+delta};return true;}
    }
    return false;
}
}
