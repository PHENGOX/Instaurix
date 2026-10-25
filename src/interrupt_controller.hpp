#pragma once

#include <cstdint>
#include <cstddef>

#include "memory_mapped_device.hpp"

class InterruptController : public MemoryMappedDevice
{
public:
    static constexpr size_t IRQ_COUNT = 32;

    InterruptController();

    size_t size() const override;

    uint8_t read8(uint64_t offset) override;
    uint16_t read16(uint64_t offset) override;
    uint32_t read32(uint64_t offset) override;
    uint64_t read64(uint64_t offset) override;

    void write8(uint64_t offset, uint8_t value) override;
    void write16(uint64_t offset, uint16_t value) override;
    void write32(uint64_t offset, uint32_t value) override;
    void write64(uint64_t offset, uint64_t value) override;

    bool hasError() const override;
    const char* getLastError() const override;
    void clearError() override;

    void raiseIRQ(size_t irq);
    void clearIRQ(size_t irq);

    bool isPending(size_t irq) const;
    bool isEnabled(size_t irq) const;
    bool isIRQActive() const;

private:
    uint32_t pending;
    uint32_t enable;

    const char* lastError;

    bool checkIRQ(size_t irq);

    uint64_t readRegister(
        uint64_t offset,
        size_t accessSize);

    void writeRegister(
        uint64_t offset,
        uint64_t value,
        size_t accessSize);
};