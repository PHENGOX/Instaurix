#include "memory.hpp"

#include <fstream>
#include <limits>
#include <algorithm>
#include <new>

uint64_t Memory::reservationVersion(uint64_t address) const
{
    if(!isValidAddress(address))return 0;
    const auto& page=writeVersions[static_cast<size_t>(address/PAGE_SIZE)];
    return page.granules?(*page.granules)[static_cast<size_t>((address%PAGE_SIZE)/16)]:page.baseline;
}

void Memory::prepareWriteVersions(uint64_t address,size_t length)
{
    const auto last=static_cast<size_t>((address+length-1)/PAGE_SIZE);
    for(size_t i=static_cast<size_t>(address/PAGE_SIZE);i<=last;++i){
        auto& page=writeVersions[i];
        if(!page.granules){
            auto granules=std::make_unique<GranuleVersions>();
            granules->fill(page.baseline);
            page.granules=std::move(granules);
        }
    }
}

void Memory::markWritten(uint64_t address, size_t length)
{
    const uint64_t version = ++writeSerial;
    for (size_t i = static_cast<size_t>(address / 16); i <= (address + length - 1) / 16; ++i)
        (*writeVersions[i/(PAGE_SIZE/16)].granules)[i%(PAGE_SIZE/16)] = version;
}

uint8_t Memory::permissionsAt(uint64_t address) const
{
    return isValidAddress(address) ? permissions[static_cast<size_t>(address / PAGE_SIZE)] : uint8_t(None);
}

bool Memory::discardPages(uint64_t address, size_t length)
{
    if (!setPermissions(address, length, None)) return false;
    const size_t first = static_cast<size_t>(address / PAGE_SIZE);
    const size_t last = static_cast<size_t>((address + length - 1) / PAGE_SIZE);
    for (size_t i = first; i <= last; ++i) pages[i].reset();
    return true;
}

bool Memory::mapReadOnlySharedPages(uint64_t address,std::span<const std::shared_ptr<uint8_t[]>> backing)
{
    if(backing.empty() || backing.size()>memorySize/PAGE_SIZE || address%PAGE_SIZE ||
       !isValidRange(address,backing.size()*PAGE_SIZE)) return false;
    for(const auto& page:backing) if(!page) return false;
    const auto first=size_t(address/PAGE_SIZE);
    for(size_t i=0;i<backing.size();++i) if(permissions[first+i]!=None || pages[first+i]) return false;
    for(size_t i=0;i<backing.size();++i) pages[first+i]=backing[i];
    return setPermissions(address,backing.size()*PAGE_SIZE,Read);
}

bool Memory::writeBytes(uint64_t address, const uint8_t* bytes, size_t length)
{
    const WriteRange range{address,bytes,length};
    return writeRanges(std::span(&range,1));
}

bool Memory::writeRanges(std::span<const WriteRange> ranges)
{
    for (const auto& range : ranges)
    {
        if (!range.bytes) { lastError = "Missing memory copy buffer"; return false; }
        if (!validateAccess(range.address,range.size,Write)) return false;
    }
    try
    {
        // Allocate before writing so a failed allocation cannot leave a partial copy.
        for (const auto& range : ranges)
        {
            const uint64_t last = (range.address + range.size - 1) / PAGE_SIZE;
            for (uint64_t i = range.address / PAGE_SIZE; i <= last; ++i) getOrCreatePage(i);
            prepareWriteVersions(range.address,range.size);
        }
    }
    catch (const std::bad_alloc&) { lastError = "Memory copy allocation failed"; return false; }
    for (const auto& range : ranges)
    {
        for (size_t i = 0; i < range.size; ++i)
            pages[static_cast<size_t>((range.address + i) / PAGE_SIZE)][(range.address + i) % PAGE_SIZE] = range.bytes[i];
        markWritten(range.address,range.size);
    }
    return true;
}

void Memory::reset(uint8_t permission)
{
    for (auto& page : pages) page.reset();
    std::fill(permissions.begin(), permissions.end(), permission & All);
    const auto version=++writeSerial;
    for(auto& page:writeVersions){page.baseline=version;page.granules.reset();}
    lastError = nullptr;
}

bool Memory::setPermissions(uint64_t address, size_t length, uint8_t permission)
{
    if ((permission & ~All) != 0 || length == 0 || !isValidRange(address, length) ||
        address % PAGE_SIZE != 0 || (length % PAGE_SIZE != 0 && address + length != memorySize))
    {
        lastError = "Invalid or unaligned permission range";
        return false;
    }
    const size_t first = static_cast<size_t>(address / PAGE_SIZE);
    const size_t last = static_cast<size_t>((address + length - 1) / PAGE_SIZE);
    std::fill(permissions.begin() + first, permissions.begin() + last + 1, permission);
    // Full-page permission changes invalidate every granule without allocating tracking rows.
    const auto version=++writeSerial;
    for(size_t i=first;i<=last;++i){writeVersions[i].baseline=version;writeVersions[i].granules.reset();}
    return true;
}

bool Memory::canAccess(uint64_t address, size_t length, uint8_t permission) const
{
    if (length == 0 || (permission & ~All) != 0 || !isValidRange(address, length)) return false;
    const size_t last = static_cast<size_t>((address + length - 1) / PAGE_SIZE);
    for (size_t page = static_cast<size_t>(address / PAGE_SIZE); page <= last; ++page)
        if ((permissions[page] & permission) != permission) return false;
    return true;
}

bool Memory::validateAccess(uint64_t address, size_t length, uint8_t permission) const
{
    if (!checkRange(address, length)) return false;
    if (!canAccess(address, length, permission))
    {
        lastError = permission == Execute ? "Memory execution denied" :
                    permission == Write ? "Memory write denied" : "Memory read denied";
        return false;
    }
    return true;
}

uint32_t Memory::fetch32(uint64_t address)
{
    if (!validateAccess(address, 4, Execute)) return 0;
    uint32_t instruction = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        const uint8_t* page = getPage((address + i) / PAGE_SIZE);
        if (page) instruction |= uint32_t(page[(address + i) % PAGE_SIZE]) << (i * 8);
    }
    return instruction;
}

Memory::Memory(size_t size)
    : memorySize(size),
      pages(
          size / PAGE_SIZE +
          ((size % PAGE_SIZE) != 0 ? 1 : 0)),
      lastError(nullptr),
      permissions(pages.size(), All),
      writeVersions(pages.size())
{
}

size_t Memory::size() const
{
    return memorySize;
}

bool Memory::loadFile(
    const char* filename,
    uint64_t address)
{
    std::ifstream file(
        filename,
        std::ios::binary | std::ios::ate);

    if (!file)
    {
        lastError = "Failed to open file";
        return false;
    }

    const std::streamsize fileSize = file.tellg();

    if (fileSize < 0)
    {
        lastError = "Invalid file size";
        return false;
    }

    const uint64_t size =
        static_cast<uint64_t>(fileSize);

    if (size > std::numeric_limits<size_t>::max())
    {
        lastError = "File is too large";
        return false;
    }

    if (!checkRange(
            address,
            static_cast<size_t>(size)))
    {
        return false;
    }

    if (!validateAccess(address, static_cast<size_t>(size), Write)) return false;
    file.seekg(0, std::ios::beg);

    for (uint64_t i = 0; i < size; ++i)
    {
        char value = 0;

        if (!file.get(value))
        {
            lastError = "Failed to read file";
            return false;
        }

        write8(
            address + i,
            static_cast<uint8_t>(
                static_cast<unsigned char>(value)));
        if (hasError()) return false;
    }

    return true;
}

bool Memory::isValidAddress(uint64_t address) const
{
    return address < memorySize;
}

bool Memory::isValidRange(
    uint64_t address,
    size_t accessSize) const
{
    if (address >= memorySize)
        return false;

    return accessSize <= memorySize - address;
}

bool Memory::checkRange(
    uint64_t address,
    size_t accessSize) const
{
    if (address >= memorySize)
    {
        lastError = "Memory address is out of bounds";
        return false;
    }

    if (accessSize > memorySize - address)
    {
        lastError = "Memory access is out of bounds";
        return false;
    }

    return true;
}

uint8_t* Memory::getPage(
    uint64_t pageIndex)
{
    if (pageIndex >= pages.size())
        return nullptr;

    return pages[pageIndex].get();
}

const uint8_t* Memory::getPage(
    uint64_t pageIndex) const
{
    if (pageIndex >= pages.size())
        return nullptr;

    return pages[pageIndex].get();
}

uint8_t* Memory::getOrCreatePage(
    uint64_t pageIndex)
{
    if (pageIndex >= pages.size())
        return nullptr;

    if (!pages[pageIndex])
    {
        pages[pageIndex] =
            std::make_shared<uint8_t[]>(PAGE_SIZE);

        for (size_t i = 0; i < PAGE_SIZE; ++i)
            pages[pageIndex][i] = 0;
    }

    return pages[pageIndex].get();
}

uint8_t Memory::read8(uint64_t address)
{
    if (!validateAccess(address, 1, Read))
        return 0;

    const uint64_t pageIndex =
        address / PAGE_SIZE;

    const size_t offset =
        static_cast<size_t>(
            address % PAGE_SIZE);

    const uint8_t* page =
        getPage(pageIndex);

    if (!page)
        return 0;

    return page[offset];
}

uint16_t Memory::read16(uint64_t address)
{
    if (!validateAccess(address, 2, Read))
        return 0;

    return static_cast<uint16_t>(read8(address)) |
           (static_cast<uint16_t>(
                read8(address + 1))
            << 8);
}

uint32_t Memory::read32(uint64_t address)
{
    if (!validateAccess(address, 4, Read))
        return 0;

    return static_cast<uint32_t>(read8(address)) |
           (static_cast<uint32_t>(
                read8(address + 1))
            << 8) |
           (static_cast<uint32_t>(
                read8(address + 2))
            << 16) |
           (static_cast<uint32_t>(
                read8(address + 3))
            << 24);
}

uint64_t Memory::read64(uint64_t address)
{
    if (!validateAccess(address, 8, Read))
        return 0;

    return static_cast<uint64_t>(read8(address)) |
           (static_cast<uint64_t>(
                read8(address + 1))
            << 8) |
           (static_cast<uint64_t>(
                read8(address + 2))
            << 16) |
           (static_cast<uint64_t>(
                read8(address + 3))
            << 24) |
           (static_cast<uint64_t>(
                read8(address + 4))
            << 32) |
           (static_cast<uint64_t>(
                read8(address + 5))
            << 40) |
           (static_cast<uint64_t>(
                read8(address + 6))
            << 48) |
           (static_cast<uint64_t>(
                read8(address + 7))
            << 56);
}

uint8_t Memory::read8(uint64_t address) const
{
    if (!validateAccess(address, 1, Read))
        return 0;

    const uint64_t pageIndex =
        address / PAGE_SIZE;

    const size_t offset =
        static_cast<size_t>(
            address % PAGE_SIZE);

    const uint8_t* page =
        getPage(pageIndex);

    if (!page)
        return 0;

    return page[offset];
}

uint16_t Memory::read16(uint64_t address) const
{
    if (!validateAccess(address, 2, Read))
        return 0;

    return static_cast<uint16_t>(read8(address)) |
           (static_cast<uint16_t>(
                read8(address + 1))
            << 8);
}

uint32_t Memory::read32(uint64_t address) const
{
    if (!validateAccess(address, 4, Read))
        return 0;

    return static_cast<uint32_t>(read8(address)) |
           (static_cast<uint32_t>(
                read8(address + 1))
            << 8) |
           (static_cast<uint32_t>(
                read8(address + 2))
            << 16) |
           (static_cast<uint32_t>(
                read8(address + 3))
            << 24);
}

uint64_t Memory::read64(uint64_t address) const
{
    if (!validateAccess(address, 8, Read))
        return 0;

    return static_cast<uint64_t>(read8(address)) |
           (static_cast<uint64_t>(
                read8(address + 1))
            << 8) |
           (static_cast<uint64_t>(
                read8(address + 2))
            << 16) |
           (static_cast<uint64_t>(
                read8(address + 3))
            << 24) |
           (static_cast<uint64_t>(
                read8(address + 4))
            << 32) |
           (static_cast<uint64_t>(
                read8(address + 5))
            << 40) |
           (static_cast<uint64_t>(
                read8(address + 6))
            << 48) |
           (static_cast<uint64_t>(
                read8(address + 7))
            << 56);
}

void Memory::write8(uint64_t address,uint8_t value)
{
    writeBytes(address,&value,1);
}

void Memory::write16(uint64_t address,uint16_t value)
{
    std::array<uint8_t,2> bytes{};
    for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(value>>(8*i));
    writeBytes(address,bytes.data(),bytes.size());
}

void Memory::write32(uint64_t address,uint32_t value)
{
    std::array<uint8_t,4> bytes{};
    for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(value>>(8*i));
    writeBytes(address,bytes.data(),bytes.size());
}

void Memory::write64(uint64_t address,uint64_t value)
{
    std::array<uint8_t,8> bytes{};
    for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(value>>(8*i));
    writeBytes(address,bytes.data(),bytes.size());
}

bool Memory::hasError() const
{
    return lastError != nullptr;
}

const char* Memory::getLastError() const
{
    return lastError;
}

void Memory::clearError()
{
    lastError = nullptr;
}

