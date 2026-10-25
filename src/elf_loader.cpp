#include "program_loader.hpp"
#include "memory.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <new>
#include <utility>

namespace {
constexpr uint64_t StackSize = 64 * 1024;
constexpr size_t MaxFileSize = 64 * 1024 * 1024;
uint64_t little(const std::vector<uint8_t>& bytes, size_t at, size_t count)
{
    uint64_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= uint64_t(bytes[at + i]) << (8 * i);
    return value;
}
bool inFile(uint64_t offset, uint64_t count, size_t size)
{
    return offset <= size && count <= size - offset;
}
bool magic(const std::vector<uint8_t>& bytes)
{
    return bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F';
}
struct Segment {
    uint64_t offset, address, fileSize, memorySize, pageBegin, pageEnd;
    uint8_t permissions;
};
}

bool ProgramLoader::loadElf(Memory& memory, const std::vector<uint8_t>& bytes, Image& image, std::string& error)
{
    error.clear();
    auto fail = [&](const char* message) { error = message; return false; };
    if (bytes.size() < 64 || bytes.size() > MaxFileSize || !magic(bytes)) return fail("En-tete ELF absent, tronque ou trop grand");
    if (bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1 || bytes[7] != 0 || bytes[8] != 0)
        return fail("ELF : seul ELF64 little-endian System V version 1 est pris en charge");
    if (little(bytes,16,2) != 2 || little(bytes,18,2) != 183 || little(bytes,20,4) != 1 || little(bytes,48,4) != 0)
        return fail("ELF : executable statique AArch64 ET_EXEC requis");
    const uint64_t entry = little(bytes,24,8), phoff = little(bytes,32,8);
    const uint64_t phnum = little(bytes,56,2);
    if (little(bytes,52,2) != 64 || little(bytes,54,2) != 56 || phnum == 0 || phnum > 128 ||
        phoff < 64 || !inFile(phoff,phnum * 56,bytes.size()))
        return fail("ELF : table des segments invalide ou tronquee");
    if (memory.size() % Memory::PAGE_SIZE != 0 || memory.size() < StackSize + 2 * Memory::PAGE_SIZE)
        return fail("ELF : RAM insuffisante ou non alignee pour la pile");
    const uint64_t stackBegin = memory.size() - StackSize;
    const uint64_t reservedBegin = stackBegin - Memory::PAGE_SIZE;
    std::vector<Segment> segments;
    bool entryValid = false;
    for (uint64_t i = 0; i < phnum; ++i)
    {
        const size_t at = static_cast<size_t>(phoff + i * 56);
        const uint64_t type = little(bytes,at,4), flags = little(bytes,at+4,4);
        const uint64_t offset = little(bytes,at+8,8), address = little(bytes,at+16,8);
        const uint64_t fileSize = little(bytes,at+32,8), memSize = little(bytes,at+40,8), align = little(bytes,at+48,8);
        if (type == 0) continue; // PT_NULL has no effect, even its other fields are unspecified.
        if (!inFile(offset,fileSize,bytes.size())) return fail("ELF : segment hors du fichier");
        if (type == 0x6474E551) // PT_GNU_STACK
        {
            if ((flags & ~uint64_t(7)) != 0 || (flags & 1) != 0 || memSize > StackSize)
                return fail("ELF : pile executable ou taille de pile non prise en charge");
            continue;
        }
        if (type == 4 || type == 6) continue; // PT_NOTE / PT_PHDR: metadata only.
        if (type != 1) return fail("ELF : type de segment non pris en charge (dynamique, TLS ou extension)");
        if (fileSize > memSize) return fail("ELF : taille fichier superieure a la taille memoire");
        if ((flags & ~uint64_t(7)) != 0 || (flags & 3) == 3)
            return fail("ELF : permissions inconnues ou segment a la fois writable et executable");
        if (align > 1 && ((align & (align - 1)) != 0 || address % align != offset % align))
            return fail("ELF : alignement ou congruence de segment invalide");
        if (memSize == 0) continue;
        if (address >= reservedBegin || memSize > reservedBegin - address)
            return fail("ELF : segment hors RAM ou dans la pile/page de garde");
        const uint64_t pageBegin = address & ~uint64_t(Memory::PAGE_SIZE - 1);
        const uint64_t pageEnd = (address + memSize + Memory::PAGE_SIZE - 1) & ~uint64_t(Memory::PAGE_SIZE - 1);
        if (pageEnd > reservedBegin) return fail("ELF : pages du segment dans la pile/page de garde");
        for (const auto& previous : segments)
            if (pageBegin < previous.pageEnd && pageEnd > previous.pageBegin)
                return fail("ELF : segments se partageant une page non pris en charge");
        if (!segments.empty() && address < segments.back().address)
            return fail("ELF : segments PT_LOAD non tries par adresse");
        const uint8_t permission = static_cast<uint8_t>(((flags & 4) ? Memory::Read : 0) |
                                   ((flags & 2) ? Memory::Write : 0) | ((flags & 1) ? Memory::Execute : 0));
        segments.push_back({offset,address,fileSize,memSize,pageBegin,pageEnd,permission});
        if ((flags & 1) && entry >= address && entry - address <= fileSize && fileSize - (entry - address) >= 4)
            entryValid = true;
    }
    if (segments.empty() || (entry & 3) != 0 || !entryValid)
        return fail("ELF : point d'entree non aligne ou absent du code executable charge");
    try
    {
        Memory staged(memory.size());
        for (const auto& segment : segments)
            for (uint64_t i = 0; i < segment.fileSize; ++i)
                staged.write8(segment.address + i,bytes[static_cast<size_t>(segment.offset + i)]);
        // New sparse pages read as zero: this also initializes all BSS bytes.
        if (!staged.setPermissions(0,staged.size(),Memory::None)) return fail(staged.getLastError());
        for (const auto& segment : segments)
            if (!staged.setPermissions(segment.pageBegin,static_cast<size_t>(segment.pageEnd-segment.pageBegin),segment.permissions))
                return fail(staged.getLastError());
        if (!staged.setPermissions(stackBegin,static_cast<size_t>(StackSize),Memory::Read | Memory::Write))
            return fail(staged.getLastError());
        if (staged.hasError()) return fail(staged.getLastError());
        Image stagedImage{Format::Elf64,entry,memory.size()};
        for (const auto& segment : segments)
            stagedImage.regions.push_back({segment.pageBegin,segment.pageEnd,segment.permissions});
        memory = std::move(staged);
        image = std::move(stagedImage);
        return true;
    }
    catch (const std::bad_alloc&) { return fail("ELF : allocation memoire impossible"); }
}

bool ProgramLoader::loadImage(Memory& memory, const char* filename, Image& image, std::string& error)
{
    error.clear();
    if (!filename) { error = "Nom de programme absent"; return false; }
    try
    {
        std::ifstream file(filename,std::ios::binary | std::ios::ate);
        const auto size = file ? file.tellg() : std::streampos(-1);
        if (size <= 0 || static_cast<uint64_t>(size) > MaxFileSize)
        { error = "Programme absent, vide ou superieur a 64 Mio"; return false; }
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0,std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        { error = "Lecture du programme incomplete"; return false; }
        auto extension = std::filesystem::path(filename).extension().string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (magic(bytes) || extension == ".elf") return loadElf(memory,bytes,image,error);
        const bool nro=bytes.size()>=20 && bytes[16]=='N' && bytes[17]=='R' && bytes[18]=='O' && bytes[19]=='0';
        if(nro || extension==".nro") return loadNro(memory,bytes,image,error);
        if(extension==".nso" || (bytes.size()>=4 && bytes[0]=='N' && bytes[1]=='S' && bytes[2]=='O' && bytes[3]=='0'))
        { error="NSO : format non pris en charge ; refus du repli vers un binaire brut";return false; }
        if(extension==".nca" || extension==".nsp" || extension==".xci")
        { error="Conteneur de contenu non pris en charge ; ce fichier n'est pas un binaire brut ARM64";return false; }
        if (bytes.size() > memory.size() || bytes.size() % 4 != 0)
        { error = "Binaire brut trop grand ou taille non multiple de 4"; return false; }
        Memory staged(memory.size());
        for (size_t i = 0; i < bytes.size(); ++i) staged.write8(i,bytes[i]);
        if (staged.hasError()) { error = staged.getLastError(); return false; }
        memory = std::move(staged);
        image = {Format::Raw,0,memory.size()};
        return true;
    }
    catch (const std::bad_alloc&) { error = "Allocation du programme impossible"; return false; }
    catch (const std::filesystem::filesystem_error&) { error = "Chemin du programme invalide"; return false; }
}
