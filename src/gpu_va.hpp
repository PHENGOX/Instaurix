#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace Gpu {
// A reservation identifier, never a CPU pointer or an implicit RAM address.
struct VirtualAddress { uint64_t value=0; };
class AddressSpace {
public:
    static constexpr uint64_t Base=0x100000000ULL,Capacity=0x4000000,PageSize=0x10000;
    static constexpr size_t Limit=32;
    enum class Result { Success,InvalidArguments,InvalidAddress,Occupied,OutOfResource };
    Result reserve(uint64_t bytes,uint64_t alignment,std::optional<VirtualAddress> fixed,VirtualAddress& output);
    Result release(VirtualAddress address,uint64_t bytes);
    Result map(uint32_t object,uint64_t objectBytes,uint64_t offset,uint64_t bytes,std::optional<VirtualAddress> fixed,VirtualAddress& output);
    Result unmap(VirtualAddress address);
    struct Translation { uint32_t object=0;uint64_t offset=0; };
    bool translate(VirtualAddress address,uint64_t bytes,Translation& output) const;
    bool references(uint32_t object) const;
    size_t mappingCount() const;
    size_t count() const;
    uint64_t reservedBytes() const;
private:
    struct Range { VirtualAddress address;uint64_t bytes=0; };
    std::array<Range,Limit> ranges{};
    struct Mapping { VirtualAddress address;uint64_t bytes=0,offset=0;uint32_t object=0;bool ownsReservation=false; };
    std::array<Mapping,Limit> mappings{};
    static bool valid(uint64_t address,uint64_t bytes);
};
}
