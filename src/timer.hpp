#pragma once

#include <cstdint>
#include <cstddef>

#include "memory_mapped_device.hpp"

class InterruptController;

class Timer : public MemoryMappedDevice
{
public:
    Timer(InterruptController& interruptController,
          size_t irqNumber);

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

    void tick(uint64_t cycles);
    void reset();

    bool isEnabled() const;
    uint64_t getCounter() const;
    uint64_t getPeriod() const;

private:
    InterruptController& interruptController;
    size_t irqNumber;

    uint64_t counter;
    uint64_t period;
    uint32_t control;

    const char* lastError;

    bool checkOffset(
        uint64_t offset,
        size_t accessSize);

    uint64_t readRegister(
        uint64_t offset,
        size_t accessSize);

    void writeRegister(
        uint64_t offset,
        uint64_t value,
        size_t accessSize);
};
