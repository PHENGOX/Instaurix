#include "timer.hpp"

#include "interrupt_controller.hpp"

Timer::Timer(
    InterruptController& interruptController,
    size_t irqNumber)
    : interruptController(interruptController),
      irqNumber(irqNumber),
      counter(0),
      period(0),
      control(0),
      lastError(nullptr)
{
}

size_t Timer::size() const
{
    return 0x18;
}

bool Timer::checkOffset(
    uint64_t offset,
    size_t accessSize)
{
    if (offset >= size())
    {
        lastError = "TIMER offset is out of bounds";
        return false;
    }

    if (accessSize > size() - offset)
    {
        lastError = "TIMER access is out of bounds";
        return false;
    }

    return true;
}

uint64_t Timer::readRegister(
    uint64_t offset,
    size_t accessSize)
{
    if (!checkOffset(offset, accessSize))
        return 0;

    if (accessSize != 8 &&
        accessSize != 4)
    {
        lastError = "Invalid TIMER access size";
        return 0;
    }

    switch (offset)
    {
        case 0x00:
            return counter;

        case 0x08:
            return period;

        case 0x10:
            if (accessSize != 4)
            {
                lastError = "Invalid access size for CONTROL TIMER";
                return 0;
            }

            return control;

        default:
            lastError = "Unknown TIMER register";
            return 0;
    }
}

void Timer::writeRegister(
    uint64_t offset,
    uint64_t value,
    size_t accessSize)
{
    if (!checkOffset(offset, accessSize))
        return;

    if (offset == 0x10)
    {
        if (accessSize != 4)
        {
            lastError = "Invalid access size for CONTROL TIMER";
            return;
        }

        control = static_cast<uint32_t>(value);

        if ((control & 1u) == 0)
        {
            counter = 0;
        }
        else if (counter == 0)
        {
            counter = period;
        }

        return;
    }

    if (accessSize != 8)
    {
        lastError = "Invalid TIMER access size";
        return;
    }

    switch (offset)
    {
        case 0x00:
            counter = value;
            return;

        case 0x08:
            period = value;

            if (counter == 0 &&
                (control & 1u) != 0)
            {
                counter = period;
            }

            return;

        default:
            lastError = "Unknown TIMER register";
            return;
    }
}

void Timer::reset()
{
    counter = 0;
    period = 0;
    control = 0;
    lastError = nullptr;
}

void Timer::tick(uint64_t cycles)
{
    if (!isEnabled())
        return;

    if (period == 0)
        return;

    if (cycles >= counter)
    {
        interruptController.raiseIRQ(irqNumber);

        counter = period - ((cycles - counter) % period);
    }
    else
    {
        counter -= cycles;
    }
}

bool Timer::isEnabled() const
{
    return (control & 1u) != 0;
}

uint64_t Timer::getCounter() const
{
    return counter;
}

uint64_t Timer::getPeriod() const
{
    return period;
}

uint8_t Timer::read8(uint64_t offset)
{
    return static_cast<uint8_t>(
        readRegister(offset, 1));
}

uint16_t Timer::read16(uint64_t offset)
{
    return static_cast<uint16_t>(
        readRegister(offset, 2));
}

uint32_t Timer::read32(uint64_t offset)
{
    return static_cast<uint32_t>(
        readRegister(offset, 4));
}

uint64_t Timer::read64(uint64_t offset)
{
    return readRegister(offset, 8);
}

void Timer::write8(
    uint64_t offset,
    uint8_t value)
{
    writeRegister(offset, value, 1);
}

void Timer::write16(
    uint64_t offset,
    uint16_t value)
{
    writeRegister(offset, value, 2);
}

void Timer::write32(
    uint64_t offset,
    uint32_t value)
{
    writeRegister(offset, value, 4);
}

void Timer::write64(
    uint64_t offset,
    uint64_t value)
{
    writeRegister(offset, value, 8);
}

bool Timer::hasError() const
{
    return lastError != nullptr;
}

const char* Timer::getLastError() const
{
    return lastError;
}

void Timer::clearError()
{
    lastError = nullptr;
}
