#pragma once

#include <cstdint>
#include <cstddef>

class MemoryMappedDevice
{
public:
    virtual ~MemoryMappedDevice() = default;

    virtual size_t size() const = 0;

    virtual uint8_t read8(uint64_t offset) = 0;
    virtual uint16_t read16(uint64_t offset) = 0;
    virtual uint32_t read32(uint64_t offset) = 0;
    virtual uint64_t read64(uint64_t offset) = 0;

    virtual void write8(uint64_t offset, uint8_t value) = 0;
    virtual void write16(uint64_t offset, uint16_t value) = 0;
    virtual void write32(uint64_t offset, uint32_t value) = 0;
    virtual void write64(uint64_t offset, uint64_t value) = 0;

    virtual bool hasError() const = 0;
    virtual const char* getLastError() const = 0;
    virtual void clearError() = 0;
};