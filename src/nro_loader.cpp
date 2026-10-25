#include "program_loader.hpp"
#include "memory.hpp"
#include <array>
#include <new>
#include <utility>

bool ProgramLoader::loadNro(Memory& memory,const std::vector<uint8_t>& bytes,Image& image,std::string& error)
{
    error.clear();
    auto fail=[&](const char* text) { error=text;return false; };
    if(bytes.size()<0x80 || bytes.size()>64*1024*1024 || bytes[16]!='N' || bytes[17]!='R' || bytes[18]!='O' || bytes[19]!='0')
        return fail("NRO : en-tete absent, tronque ou trop grand");
    auto read=[&](size_t at) { uint32_t value=0;for(size_t i=0;i<4;++i) value|=uint32_t(bytes[at+i])<<(8*i);return value; };
    if(read(0x14)!=0 || read(0x1C)!=0 || read(0x3C)!=0)
        return fail("NRO : version, flags ou en-tete additionnel non pris en charge");
    const uint64_t size=read(0x18), bss=read(0x38), module=read(4);
    if(size!=bytes.size()) return fail("NRO : taille declaree differente du fichier (assets non pris en charge)");
    // Extended dynamic metadata is outside the selected static profile.
    for(size_t at=0x60;at<0x80;at+=4) if(read(at)) return fail("NRO : metadonnees dynamiques etendues non prises en charge");
    constexpr uint64_t base=0x10000, stackSize=0x10000, page=Memory::PAGE_SIZE;
    if(memory.size()%page || memory.size()<base+stackSize+2*page) return fail("NRO : RAM insuffisante ou non alignee");
    const uint64_t guard=memory.size()-stackSize-page;
    struct Segment { uint64_t offset,size,memorySize; uint8_t permissions; };
    std::array<Segment,3> segments{};
    for(size_t i=0;i<3;++i) {
        auto& segment=segments[i];
        segment.offset=read(0x20+i*8);segment.size=read(0x24+i*8);
        segment.memorySize=segment.size+(i==2?bss:0);
        segment.permissions=i==0 ? Memory::Read|Memory::Execute : i==1 ? Memory::Read : Memory::Read|Memory::Write;
        if(segment.offset%page || segment.size%page || segment.offset>size || segment.size>size-segment.offset)
            return fail("NRO : segments non alignes ou hors fichier");
        if(segment.memorySize && (base+segment.offset>=guard || segment.memorySize>guard-base-segment.offset))
            return fail("NRO : segment ou BSS hors RAM ou dans la pile");
    }
    if(segments[0].offset || segments[0].size<page) return fail("NRO : segment text doit commencer a zero et contenir l'en-tete");
    for(size_t i=0;i<3;++i) for(size_t j=i+1;j<3;++j)
        if(segments[i].memorySize && segments[j].memorySize && segments[i].offset<segments[j].offset+segments[j].memorySize &&
            segments[j].offset<segments[i].offset+segments[i].memorySize) return fail("NRO : segments ou BSS en chevauchement");
    if(module) {
        bool mapped=false;
        for(const auto& segment:segments)
            if(module>=segment.offset && module-segment.offset<=segment.size && 28<=segment.size-(module-segment.offset)) mapped=true;
        if(module%4 || !mapped || module<0x80 || read(size_t(module))!=0x30444F4D)
            return fail("NRO : pointeur ou signature MOD0 invalide");
    }
    try {
        Memory staged(memory.size());staged.reset(Memory::None);
        Image next;next.format=Format::Nro;next.entry=base;next.stack=memory.size();next.requiresNroRuntime=module!=0;
        next.nroModuleOffset=module;next.nroFileSize=size;
        next.nroBssBegin=base+segments[2].offset+segments[2].size;next.nroBssEnd=next.nroBssBegin+bss;
        for(const auto& segment:segments) if(segment.memorySize) {
            const uint64_t address=base+segment.offset, length=(segment.memorySize+page-1)&~(page-1);
            if(!staged.setPermissions(address,size_t(length),Memory::Read|Memory::Write)) return fail("NRO : mapping impossible");
            if(segment.size && !staged.writeBytes(address,bytes.data()+segment.offset,size_t(segment.size))) return fail("NRO : copie de segment impossible");
            if(!staged.setPermissions(address,size_t(length),segment.permissions)) return fail("NRO : protection des segments impossible");
            next.regions.push_back({address,address+length,segment.permissions});
        }
        if(!staged.setPermissions(memory.size()-stackSize,stackSize,Memory::Read|Memory::Write)) return fail("NRO : pile impossible");
        memory=std::move(staged);image=std::move(next);return true;
    }
    catch(const std::bad_alloc&) { return fail("NRO : allocation impossible"); }
}
