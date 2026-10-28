#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <span>
#include <array>

class Memory
{
public:
    static constexpr size_t PAGE_SIZE = 0x1000; // 4 KiB

    explicit Memory(size_t size);
    Memory(const Memory&)=delete;
    Memory& operator=(const Memory&)=delete;
    Memory(Memory&&) noexcept=default;
    Memory& operator=(Memory&&) noexcept=default;
    using SharedPages=std::vector<std::shared_ptr<uint8_t[]>>;
    bool mapReadOnlySharedPages(uint64_t address,std::span<const std::shared_ptr<uint8_t[]>> backing);
    enum Permission : uint8_t { None = 0, Read = 1, Write = 2, Execute = 4, All = 7 };
    void reset(uint8_t permission = All);
    bool setPermissions(uint64_t address, size_t size, uint8_t permission);
    bool canAccess(uint64_t address, size_t size, uint8_t permission) const;
    bool validateAccess(uint64_t address, size_t size, uint8_t permission) const;
    uint32_t fetch32(uint64_t address);
    uint8_t permissionsAt(uint64_t address) const;
    bool discardPages(uint64_t address, size_t size);
    bool writeBytes(uint64_t address, const uint8_t* bytes, size_t size);
    struct WriteRange { uint64_t address; const uint8_t* bytes; size_t size; };
    bool writeRanges(std::span<const WriteRange> ranges);
    // 16-byte reservation granules, shared by every CPU using this RAM.
    uint64_t reservationVersion(uint64_t address) const;

    size_t size() const;

    bool loadFile(
        const char* filename,
        uint64_t address);

    bool isValidAddress(uint64_t address) const;

    bool isValidRange(
        uint64_t address,
        size_t size) const;

    uint8_t read8(uint64_t address);
    uint16_t read16(uint64_t address);
    uint32_t read32(uint64_t address);
    uint64_t read64(uint64_t address);

    uint8_t read8(uint64_t address) const;
    uint16_t read16(uint64_t address) const;
    uint32_t read32(uint64_t address) const;
    uint64_t read64(uint64_t address) const;

    void write8(
        uint64_t address,
        uint8_t value);

    void write16(
        uint64_t address,
        uint16_t value);

    void write32(
        uint64_t address,
        uint32_t value);

    void write64(
        uint64_t address,
        uint64_t value);

    bool hasError() const;
    const char* getLastError() const;
    void clearError();

private:
    size_t memorySize;

    SharedPages pages;

    mutable const char* lastError;
    std::vector<uint8_t> permissions;
    using GranuleVersions=std::array<uint64_t,PAGE_SIZE/16>;
    struct ReservationPage { uint64_t baseline=0;std::unique_ptr<GranuleVersions> granules; };
    std::vector<ReservationPage> writeVersions;
    uint64_t writeSerial = 0;
    void prepareWriteVersions(uint64_t address,size_t length);
    void markWritten(uint64_t address, size_t length);

    bool checkRange(
        uint64_t address,
        size_t accessSize) const;

    uint8_t* getPage(
        uint64_t pageIndex);

    const uint8_t* getPage(
        uint64_t pageIndex) const;

    uint8_t* getOrCreatePage(
        uint64_t pageIndex);
};
