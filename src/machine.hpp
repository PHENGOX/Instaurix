#pragma once
#include "memory.hpp"
#include "bus.hpp"
#include "cpu.hpp"
#include "interrupt_controller.hpp"
#include "timer.hpp"
#include "test_device.hpp"
#include "process.hpp"
#include <string>

// Development machine: these addresses are not a Switch 2 hardware map.
class Machine
{
public:
    static constexpr uint64_t RAM_SIZE = 16 * 1024 * 1024;
    static constexpr uint64_t DEVICE_BASE = 0x10000000;
    static constexpr uint64_t IRQ_BASE = 0x10001000;
    static constexpr uint64_t TIMER_BASE = 0x10002000;
    static constexpr size_t TIMER_IRQ = 4;
    enum class Stop { Budget, Interrupt, Waiting, Frame, Exited, Error };
    struct Result { Stop stop; uint64_t instructions; uint64_t exitCode = 0; };
    explicit Machine(bool enableTestProtocol = false, bool enableReferenceHle = false, bool enableHomebrewEntry = false);
    bool load(const char* path);
    bool setAppletFocus(bool focused) { return !exited && processContext.state()!=Process::State::Faulted && processContext.setAppletFocus(focused); }
    bool mountReadOnlyRoot(const std::filesystem::path& path);
    Result run(uint64_t maxInstructions, bool stopOnFrame = false);
    bool setButtons(uint32_t buttons) {
        return referenceHleEnabled && !exited && processContext.state()!=Process::State::Faulted && processContext.setButtons(buttons);
    }
    uint32_t signalEvent(uint32_t handle) { return processContext.signalEvent(handle); }
    bool advanceTime(uint64_t nanoseconds) {
        return referenceHleEnabled && !exited && processContext.state() != Process::State::Faulted &&
            processContext.advanceTime(nanoseconds);
    }
    const std::string& error() const { return lastError; }
    const Process& process() const { return processContext; }
    Memory ram;
    Bus bus;
    CPU cpu;
    InterruptController interrupts;
    Timer timer;
    TestDevice device;
private:
    std::string lastError;
    bool testProtocolEnabled;
    bool exited = false;
    uint64_t exitCode = 0;
    bool referenceHleEnabled;
    bool homebrewEntryEnabled;
    Process processContext;
    uint64_t quantumUsed = 0;
    std::shared_ptr<const ReadOnlyFiles> mountedFiles;
};
