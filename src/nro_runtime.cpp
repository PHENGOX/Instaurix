#include "program_loader.hpp"
#include "memory.hpp"
#include <array>
#include <set>

// Validation only. The guest's unchanged __nx_dynamic performs the relocations.
bool ProgramLoader::validateNroRuntime(const Memory& ram,const Image& image,std::string& error,RuntimeLayout* layout)
{
    error.clear();
    RuntimeLayout next;
    auto fail=[&](const char* text) { error=std::string("NRO MOD0: ")+text;return false; };
    if(image.format!=Format::Nro || !image.requiresNroRuntime) return fail("homebrew profile requires MOD0");
    const uint64_t base=image.entry;
    if(base>ram.size() || image.nroModuleOffset>ram.size()-base || image.nroFileSize>ram.size()-base)
        return fail("image is outside RAM");
    const uint64_t module=base+image.nroModuleOffset;
    auto contains=[&](uint64_t address,uint64_t size,bool writable,bool file) {
        if(!size || address<base || address>ram.size() || size>ram.size()-address) return false;
        if(file && (address-base>image.nroFileSize || size>image.nroFileSize-(address-base))) return false;
        if(!ram.canAccess(address,size_t(size),writable ? Memory::Read|Memory::Write : Memory::Read)) return false;
        for(const auto& r:image.regions)
            if(address>=r.begin && address<=r.end && size<=r.end-address &&
               (r.permission&Memory::Read) && bool(r.permission&Memory::Write)==writable) return true;
        return false;
    };
    if(!contains(module,52,false,true) || ram.read32(module)!=0x30444F4D) return fail("header is unreadable or writable");
    auto relative=[&](uint64_t origin,uint32_t raw,uint64_t& address) {
        const int64_t offset=raw<0x80000000U ? int64_t(raw) : int64_t(raw)-0x100000000LL;
        if(offset<0 && origin<uint64_t(-offset)) return false;
        if(offset>=0 && uint64_t(offset)>ram.size()-origin) return false;
        address=origin+uint64_t(offset);return address<=ram.size();
    };
    uint64_t dynamic=0,bssBegin=0,bssEnd=0,ehBegin=0,ehEnd=0;
    if(!relative(module,ram.read32(module+4),dynamic) || dynamic%8 ||
       !relative(module,ram.read32(module+8),bssBegin) || !relative(module,ram.read32(module+12),bssEnd) ||
       bssBegin!=image.nroBssBegin || bssEnd!=image.nroBssEnd ||
       (bssEnd>bssBegin && !contains(bssBegin,bssEnd-bssBegin,true,false))) return fail("invalid dynamic or BSS offsets");
    if(!relative(module,ram.read32(module+16),ehBegin) || !relative(module,ram.read32(module+20),ehEnd) ||
       ehEnd<ehBegin || (ehEnd>ehBegin && !contains(ehBegin,ehEnd-ehBegin,false,true)) || ram.read32(module+24))
        return fail("invalid exception metadata or reserved field");
    if(ram.read32(module+28)==0x30594E4C || ram.read32(module+40)==0x31594E4C) {
        if(!contains(module,64,false,true) || ram.read32(module+28)!=0x30594E4C ||
           ram.read32(module+40)!=0x31594E4C || ram.read32(module+52)!=0x32594E4C ||
           ram.read32(module+56)!=1 || ram.read32(module+60)) return fail("incomplete LNY extensions or unknown version");
        uint64_t gotBegin=0,gotEnd=0;
        if(!relative(module,ram.read32(module+44),next.relroBegin) ||
           !relative(module,ram.read32(module+48),next.relroEnd) ||
           next.relroBegin%Memory::PAGE_SIZE || next.relroEnd%Memory::PAGE_SIZE || next.relroEnd<=next.relroBegin ||
           !contains(next.relroBegin,next.relroEnd-next.relroBegin,true,true) ||
           !relative(module,ram.read32(module+32),gotBegin) || !relative(module,ram.read32(module+36),gotEnd) ||
           gotBegin%8 || gotEnd%8 || gotEnd<gotBegin || gotBegin<next.relroBegin || gotEnd>next.relroEnd)
            return fail("invalid RELRO or GOT ranges");
    }
    std::array<uint64_t,6> values{};
    std::array<bool,6> seen{};
    bool terminated=false;
    for(uint64_t at=dynamic;contains(at,16,false,true);at+=16) {
        const auto tag=ram.read64(at), value=ram.read64(at+8);
        if(!tag) { terminated=true;break; }
        size_t index=0;
        switch(tag) {
            case 7:index=0;break; case 8:index=1;break; case 9:index=2;break;
            case 36:index=3;break; case 35:index=4;break; case 37:index=5;break;
            default:return fail("unsupported dynamic tag");
        }
        if(seen[index]) return fail("duplicate dynamic tag");
        seen[index]=true;values[index]=value;
    }
    if(!terminated) return fail("dynamic table has no terminator within the file");
    std::set<uint64_t> targets;
    auto target=[&](uint64_t offset) {
        return offset<=ram.size()-base && contains(base+offset,8,true,false) && !(offset&7) && targets.insert(offset).second;
    };
    for(size_t group:{size_t(0),size_t(3)}) {
        const uint64_t stride=group ? 8 : 24;
        if(seen[group]!=seen[group+1] || (seen[group+2] && values[group+2]!=stride) || values[group+1]%stride)
            return fail("inconsistent relocation table description");
        if(!seen[group]) { if(seen[group+2]) return fail("entry size without table");continue; }
        const auto offset=values[group], size=values[group+1];
        if(offset>ram.size()-base || (offset&7) || (size && !contains(base+offset,size,false,true)))
            return fail("relocation table is outside the file or writable");
        uint64_t cursor=0;bool hasCursor=false;
        for(uint64_t at=base+offset,end=at+size;at<end;at+=stride) {
            const auto word=ram.read64(at);
            if(!group) {
                const auto info=ram.read64(at+8), addend=ram.read64(at+16);
                if(info==0) continue;
                if(info!=1027) return fail("unsupported RELA relocation type or symbol");
                // Signed addends must produce an address in the bounded local RAM.
                if((addend<=INT64_MAX && addend>ram.size()-base) ||
                   (addend>INT64_MAX && (~addend+1)>base) || !target(word))
                    return fail("invalid RELA target or addend");
            } else if(!(word&1)) {
                if(!target(word) || ram.read64(base+word)>ram.size()-base) return fail("invalid RELR target");
                cursor=word+8;hasCursor=true;
            } else {
                if(!hasCursor || cursor>ram.size()-base || 63*8>ram.size()-base-cursor)
                    return fail("RELR bitmap has no base or is outside RAM");
                for(unsigned bit=1;bit<64;++bit) if(word&(uint64_t(1)<<bit)) {
                    const auto slot=cursor+(bit-1)*8;
                    if(!target(slot) || ram.read64(base+slot)>ram.size()-base) return fail("invalid RELR bitmap target");
                }
                cursor+=63*8;
            }
        }
    }
    if(layout) *layout=next;
    return true;
}
