#pragma once

#include "memory.hpp"
#include "memory_mapped_device.hpp"

// Adapts the existing byte-addressable Memory to a mapped Bus device.
class MemoryBackedDevice final : public MemoryMappedDevice
{
public:
    explicit MemoryBackedDevice(Memory& memory) : memory(memory) {}
    uint8_t read8(uint64_t offset) override { return memory.read8(offset); }
    uint16_t read16(uint64_t offset) override { return memory.read16(offset); }
    uint32_t read32(uint64_t offset) override { return memory.read32(offset); }
    uint64_t read64(uint64_t offset) override { return memory.read64(offset); }
    void write8(uint64_t offset, uint8_t value) override { memory.write8(offset, value); }
    void write16(uint64_t offset, uint16_t value) override { memory.write16(offset, value); }
    void write32(uint64_t offset, uint32_t value) override { memory.write32(offset, value); }
    void write64(uint64_t offset, uint64_t value) override { memory.write64(offset, value); }
    size_t size() const override { return memory.size(); }
    bool hasError() const override { return memory.hasError(); }
    const char* getLastError() const override { return memory.getLastError(); }
    void clearError() override { memory.clearError(); }
private:
    Memory& memory;
};
