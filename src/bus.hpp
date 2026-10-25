#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

class Memory;
class MemoryMappedDevice;

struct MemoryRegion
{
    uint64_t start;
    uint64_t end;
    MemoryMappedDevice* device;
    const char* name;
};

class Bus
{
public:
    explicit Bus(Memory& memory);

    bool mapDevice(
        uint64_t start,
        uint64_t size,
        MemoryMappedDevice& device,
        const char* name);

    bool isValidRange(uint64_t address, size_t size) const;
    bool isRamRange(uint64_t address, size_t size) const;
    bool validateRamAccess(uint64_t address, size_t size, uint8_t permission);
    uint32_t fetch32(uint64_t address);
    uint8_t read8(uint64_t address);
    uint16_t read16(uint64_t address);
    uint32_t read32(uint64_t address);
    uint64_t read64(uint64_t address);

    void write8(uint64_t address, uint8_t value);
    void write16(uint64_t address, uint16_t value);
    void write32(uint64_t address, uint32_t value);
    void write64(uint64_t address, uint64_t value);

    bool hasError() const;
    const char* getLastError() const;
    void clearError();

    Memory& getMemory();
    const Memory& getMemory() const;

private:
    Memory& memory;

    std::vector<MemoryRegion> regions;

    const char* lastError;

    MemoryMappedDevice* findDevice(
        uint64_t address,
        size_t accessSize,
        uint64_t& deviceOffset);

    bool checkRamAccess(
        uint64_t address,
        size_t size, uint8_t permission);

    bool checkRegionOverlap(
        uint64_t start,
        uint64_t end) const;
};
