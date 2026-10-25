#include "interrupt_controller.hpp"

InterruptController::InterruptController()
    : pending(0),
      enable(0),
      lastError(nullptr)
{
}

size_t InterruptController::size() const
{
    return 0x0C;
}

bool InterruptController::checkIRQ(size_t irq)
{
    if (irq >= IRQ_COUNT)
    {
        lastError = "Numero IRQ invalide";
        return false;
    }

    return true;
}

void InterruptController::raiseIRQ(size_t irq)
{
    if (!checkIRQ(irq))
        return;

    pending |= (1u << irq);
}

void InterruptController::clearIRQ(size_t irq)
{
    if (!checkIRQ(irq))
        return;

    pending &= ~(1u << irq);
}

bool InterruptController::isPending(size_t irq) const
{
    if (irq >= IRQ_COUNT)
        return false;

    return (pending & (1u << irq)) != 0;
}

bool InterruptController::isEnabled(size_t irq) const
{
    if (irq >= IRQ_COUNT)
        return false;

    return (enable & (1u << irq)) != 0;
}

bool InterruptController::isIRQActive() const
{
    return (pending & enable) != 0;
}

uint64_t InterruptController::readRegister(
    uint64_t offset,
    size_t accessSize)
{
    if (offset >= size())
    {
        lastError = "Offset IRQ hors limites";
        return 0;
    }

    if (accessSize > size() - offset)
    {
        lastError = "Acces IRQ hors limites";
        return 0;
    }

    if (accessSize != 4)
    {
        lastError = "Taille IRQ invalide";
        return 0;
    }

    switch (offset)
    {
        case 0x00:
            return pending;

        case 0x04:
            return enable;

        case 0x08:
            return pending & enable;

        default:
            lastError = "Registre IRQ inconnu";
            return 0;
    }
}

void InterruptController::writeRegister(
    uint64_t offset,
    uint64_t value,
    size_t accessSize)
{
    if (offset >= size())
    {
        lastError = "Offset IRQ hors limites";
        return;
    }

    if (accessSize > size() - offset)
    {
        lastError = "Acces IRQ hors limites";
        return;
    }

    if (accessSize != 4)
    {
        lastError = "Taille IRQ invalide";
        return;
    }

    switch (offset)
    {
        case 0x00:
            pending &= ~static_cast<uint32_t>(value);
            return;

        case 0x04:
            enable = static_cast<uint32_t>(value);
            return;

        case 0x08:
            lastError = "Registre IRQ ACTIVE en lecture seule";
            return;

        default:
            lastError = "Registre IRQ inconnu";
            return;
    }
}

uint8_t InterruptController::read8(uint64_t offset)
{
    return static_cast<uint8_t>(
        readRegister(offset, 1));
}

uint16_t InterruptController::read16(uint64_t offset)
{
    return static_cast<uint16_t>(
        readRegister(offset, 2));
}

uint32_t InterruptController::read32(uint64_t offset)
{
    return static_cast<uint32_t>(
        readRegister(offset, 4));
}

uint64_t InterruptController::read64(uint64_t offset)
{
    return readRegister(offset, 8);
}

void InterruptController::write8(
    uint64_t offset,
    uint8_t value)
{
    writeRegister(offset, value, 1);
}

void InterruptController::write16(
    uint64_t offset,
    uint16_t value)
{
    writeRegister(offset, value, 2);
}

void InterruptController::write32(
    uint64_t offset,
    uint32_t value)
{
    writeRegister(offset, value, 4);
}

void InterruptController::write64(
    uint64_t offset,
    uint64_t value)
{
    writeRegister(offset, value, 8);
}

bool InterruptController::hasError() const
{
    return lastError != nullptr;
}

const char* InterruptController::getLastError() const
{
    return lastError;
}

void InterruptController::clearError()
{
    lastError = nullptr;
}