#include "test_device.hpp"

TestDevice::TestDevice()
    : control(0),
      data(0),
      lastError(nullptr)
{
}

size_t TestDevice::size() const
{
    return 0x10;
}

bool TestDevice::checkOffset(uint64_t offset, size_t accessSize)
{
    if (offset >= size())
    {
        lastError = "MMIO offset is out of bounds";
        return false;
    }

    if (accessSize > size() - offset)
    {
        lastError = "MMIO access is out of bounds";
        return false;
    }

    return true;
}

uint64_t TestDevice::readRegister(
    uint64_t offset,
    size_t accessSize)
{
    if (!checkOffset(offset, accessSize))
        return 0;

    switch (offset)
    {
        case 0x00:
            if (accessSize != 4)
            {
                lastError = "Invalid access size for ID";
                return 0;
            }

            return DEVICE_ID;

        case 0x04:
            if (accessSize != 4)
            {
                lastError = "Invalid access size for CONTROL";
                return 0;
            }

            return control;

        case 0x08:
            if (accessSize != 4)
            {
                lastError = "Invalid access size for STATUS";
                return 0;
            }

            return control != 0 ? 1 : 0;

        case 0x0C:
            if (accessSize != 4)
            {
                lastError = "Invalid access size for DATA";
                return 0;
            }

            return data;

        default:
            lastError = "Unknown MMIO register";
            return 0;
    }
}

void TestDevice::writeRegister(
    uint64_t offset,
    uint64_t value,
    size_t accessSize)
{
    if (!checkOffset(offset, accessSize))
        return;

    if (accessSize != 4)
    {
        lastError = "Invalid MMIO access size";
        return;
    }

    switch (offset)
    {
        case 0x00:
            lastError = "ID register is read-only";
            return;

        case 0x04:
            control = static_cast<uint32_t>(value);
            return;

        case 0x08:
            lastError = "STATUS register is read-only";
            return;

        case 0x0C:
            data = static_cast<uint32_t>(value);
            return;

        default:
            lastError = "Unknown MMIO register";
            return;
    }
}

uint8_t TestDevice::read8(uint64_t offset)
{
    return static_cast<uint8_t>(
        readRegister(offset, 1));
}

uint16_t TestDevice::read16(uint64_t offset)
{
    return static_cast<uint16_t>(
        readRegister(offset, 2));
}

uint32_t TestDevice::read32(uint64_t offset)
{
    return static_cast<uint32_t>(
        readRegister(offset, 4));
}

uint64_t TestDevice::read64(uint64_t offset)
{
    return readRegister(offset, 8);
}

void TestDevice::write8(
    uint64_t offset,
    uint8_t value)
{
    writeRegister(offset, value, 1);
}

void TestDevice::write16(
    uint64_t offset,
    uint16_t value)
{
    writeRegister(offset, value, 2);
}

void TestDevice::write32(
    uint64_t offset,
    uint32_t value)
{
    writeRegister(offset, value, 4);
}

void TestDevice::write64(
    uint64_t offset,
    uint64_t value)
{
    writeRegister(offset, value, 8);
}

bool TestDevice::hasError() const
{
    return lastError != nullptr;
}

const char* TestDevice::getLastError() const
{
    return lastError;
}

void TestDevice::clearError()
{
    lastError = nullptr;
}