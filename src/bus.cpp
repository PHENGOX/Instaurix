#include "bus.hpp"

#include "memory.hpp"
#include "memory_mapped_device.hpp"

Bus::Bus(Memory& memory)
    : memory(memory),
      lastError(nullptr)
{
}

bool Bus::checkRegionOverlap(
    uint64_t start,
    uint64_t end) const
{
    for (const MemoryRegion& region : regions)
    {
        if (start <= region.end &&
            end >= region.start)
        {
            return true;
        }
    }

    return false;
}

bool Bus::mapDevice(
    uint64_t start,
    uint64_t size,
    MemoryMappedDevice& device,
    const char* name)
{
    if (size == 0)
    {
        lastError = "Invalid region size";
        return false;
    }

    if (name == nullptr)
    {
        lastError = "Invalid region name";
        return false;
    }

    if (device.size() < size)
    {
        lastError = "Device is too small for the region";
        return false;
    }

    uint64_t end = start + size - 1;

    if (end < start)
    {
        lastError = "Region address overflow";
        return false;
    }

    if (checkRegionOverlap(start, end))
    {
        lastError = "Overlapping MMIO regions";
        return false;
    }

    regions.push_back(
        {
            start,
            end,
            &device,
            name
        });

    return true;
}

MemoryMappedDevice* Bus::findDevice(
    uint64_t address,
    size_t accessSize,
    uint64_t& deviceOffset)
{
    for (MemoryRegion& region : regions)
    {
        if (address >= region.start &&
            address <= region.end)
        {
            uint64_t offset = address - region.start;

            if (accessSize > region.end - address + 1)
            {
                lastError = "MMIO access crosses the region boundary";
                return nullptr;
            }

            deviceOffset = offset;

            return region.device;
        }
    }

    return nullptr;
}

bool Bus::isValidRange(uint64_t address, size_t size) const
{
    if (size == 0 || size - 1 > UINT64_MAX - address) return false;
    const uint64_t end = address + size - 1;
    for (const auto& region : regions)
    {
        if (address <= region.end && end >= region.start)
            return address >= region.start && end <= region.end;
    }
    return memory.isValidRange(address, size);
}

bool Bus::isRamRange(uint64_t address, size_t size) const
{
    if (size == 0 || !memory.isValidRange(address, size)) return false;
    const uint64_t end = address + size - 1;
    for (const auto& region : regions)
        if (address <= region.end && end >= region.start) return false;
    return true;
}

bool Bus::checkRamAccess(
    uint64_t address,
    size_t size, uint8_t permission)
{
    if (!isValidRange(address, size))
    {
        lastError = "Bus access is out of bounds or crosses an MMIO region";
        return false;
    }

    if (!memory.validateAccess(address, size, permission))
    {
        lastError = memory.getLastError();
        return false;
    }
    return true;
}

bool Bus::validateRamAccess(uint64_t address, size_t size, uint8_t permission)
{
    if (!isRamRange(address, size))
    {
        lastError = "Expected RAM access is out of bounds or targets MMIO";
        return false;
    }
    return checkRamAccess(address, size, permission);
}

uint32_t Bus::fetch32(uint64_t address)
{
    if (!validateRamAccess(address, 4, Memory::Execute)) return 0;
    return memory.fetch32(address);
}

uint8_t Bus::read8(uint64_t address)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 1, offset))
    {
        uint8_t value = device->read8(offset);

        if (device->hasError())
            lastError = device->getLastError();

        return value;
    }

    if (!checkRamAccess(address, 1, Memory::Read))
        return 0;

    return memory.read8(address);
}

uint16_t Bus::read16(uint64_t address)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 2, offset))
    {
        uint16_t value = device->read16(offset);

        if (device->hasError())
            lastError = device->getLastError();

        return value;
    }

    if (!checkRamAccess(address, 2, Memory::Read))
        return 0;

    return memory.read16(address);
}

uint32_t Bus::read32(uint64_t address)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 4, offset))
    {
        uint32_t value = device->read32(offset);

        if (device->hasError())
            lastError = device->getLastError();

        return value;
    }

    if (!checkRamAccess(address, 4, Memory::Read))
        return 0;

    return memory.read32(address);
}

uint64_t Bus::read64(uint64_t address)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 8, offset))
    {
        uint64_t value = device->read64(offset);

        if (device->hasError())
            lastError = device->getLastError();

        return value;
    }

    if (!checkRamAccess(address, 8, Memory::Read))
        return 0;

    return memory.read64(address);
}

void Bus::write8(
    uint64_t address,
    uint8_t value)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 1, offset))
    {
        device->write8(offset, value);

        if (device->hasError())
            lastError = device->getLastError();

        return;
    }

    if (!checkRamAccess(address, 1, Memory::Write))
        return;

    memory.write8(address, value);
}

void Bus::write16(
    uint64_t address,
    uint16_t value)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 2, offset))
    {
        device->write16(offset, value);

        if (device->hasError())
            lastError = device->getLastError();

        return;
    }

    if (!checkRamAccess(address, 2, Memory::Write))
        return;

    memory.write16(address, value);
}

void Bus::write32(
    uint64_t address,
    uint32_t value)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 4, offset))
    {
        device->write32(offset, value);

        if (device->hasError())
            lastError = device->getLastError();

        return;
    }

    if (!checkRamAccess(address, 4, Memory::Write))
        return;

    memory.write32(address, value);
}

void Bus::write64(
    uint64_t address,
    uint64_t value)
{
    uint64_t offset = 0;

    if (MemoryMappedDevice* device =
            findDevice(address, 8, offset))
    {
        device->write64(offset, value);

        if (device->hasError())
            lastError = device->getLastError();

        return;
    }

    if (!checkRamAccess(address, 8, Memory::Write))
        return;

    memory.write64(address, value);
}

bool Bus::hasError() const
{
    return lastError != nullptr || memory.hasError();
}

const char* Bus::getLastError() const
{
    return lastError ? lastError : memory.getLastError();
}

void Bus::clearError()
{
    lastError = nullptr;
    memory.clearError();

    for (MemoryRegion& region : regions)
    {
        region.device->clearError();
    }
}

Memory& Bus::getMemory()
{
    return memory;
}

const Memory& Bus::getMemory() const
{
    return memory;
}

