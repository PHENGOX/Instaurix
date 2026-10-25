#pragma once
#include "cpu.hpp"
#include "program_loader.hpp"
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <deque>
#include "read_only_files.hpp"
#include "frame.hpp"
#include "input_state.hpp"

// Local two-thread development runtime. Reference ABI, not validated S2.
class Process
{
public:
    enum class State { Ready, Running, Paused, Waiting, Exited, Faulted };
    struct Thread { uint64_t id = 1, localRegion = 0; State state = State::Ready; CPU::Context context; };
    enum class Action { Continue, Exit, Wait, Yield, ThreadExit, Unsupported };
    static constexpr uint32_t InvalidHandle = 0xE401, TimedOut = 0xEA01,
        InvalidState = 0xFA01, OutOfHandles = 0xD201, TooManyHandles = 0xEE01;
    uint32_t signalEvent(uint32_t handle);
    bool resumeWait(CPU& cpu);
    bool schedule(CPU& cpu, bool rotate = false);
    bool allThreadsExited() const;
    bool advanceTime(uint64_t nanoseconds);
    uint64_t virtualTime() const { return virtualNanoseconds; }
    std::optional<uint64_t> nextWakeDelay() const;
    const Thread& workerThread() const { return worker; }
    size_t activeThreadIndex() const { return active; }
    static constexpr uint64_t WorkerStackBase = 0xC00000, WorkerStackTop = 0xC10000,
        WorkerTlsPage = 0xC11000;
    static constexpr uint64_t Quantum = 64;
    static constexpr size_t DomainObjectLimit = 64;
    static constexpr size_t AppletMessageLimit = 32;
    uint64_t appletResourceId() const { return applet.id; }
    bool appletActive() const { return applet.live; }
    bool appletFocused() const { return applet.focused; }
    bool setAppletFocus(bool focused);
    uint64_t systemTick() const { return (virtualNanoseconds/625)*12+((virtualNanoseconds%625)*12)/625; }
    static constexpr uint64_t HeapBase = 0x200000, HeapCapacity = 0x800000, HeapAlignment = 0x200000;
    static constexpr uint64_t StackSize = 0x10000;
    static constexpr uint32_t Success = 0, InvalidSize = 0xCA01, InvalidAddress = 0xCC01,
        OutOfResource = 0xCE01, OutOfMemory = 0xD001, InvalidPointer = 0xE601;

    bool initialize(Memory& ram, const ProgramLoader::Image& image, bool reference, std::string& error, bool homebrew = false);
    static constexpr uint64_t HomebrewEnvironment = 0xD00000, HomebrewReturn = 0xD01000;
    static constexpr uint64_t AliasBase = HeapBase+HeapCapacity, AliasSize = WorkerStackBase-AliasBase;
    uint32_t mainHandle() const { return mainThreadHandle; }
    Action dispatch(uint16_t id, CPU& cpu, Bus& bus);
    void checkpoint(const CPU& cpu, State next);
    State state() const { return processState; }
    const Thread& mainThread() const { return thread; }
    uint64_t heapSize() const { return currentHeapSize; }
    bool referenceEnabled() const { return enabled; }
    void setReadOnlyFiles(std::shared_ptr<const ReadOnlyFiles> files) { readOnlyFiles = std::move(files); }
    const std::shared_ptr<const Frame>& frame() const { return lastFrame; }
    uint64_t frameCount() const { return presentedFrames; }
    bool setButtons(uint32_t buttons) {
        if(buttons & ~InputState::Mask) return false;
        inputButtons=buttons;return true;
    }
private:
    State processState = State::Ready;
    Thread thread;
    Thread worker;
    size_t active = 0;
    std::array<bool,2> started{true,false};
    bool workerCreated = false;
    uint64_t currentHeapSize = 0;
    bool enabled = false;
    bool homebrewProfile = false;
    std::vector<uint32_t> types;
    std::vector<uint32_t> memoryAttributes;
    ProgramLoader::RuntimeLayout runtimeLayout;
    struct Event { bool signaled = false, serviceManaged = false; };
    struct AppletState {
        uint64_t id=0;
        bool live=false, focused=true;
        std::deque<uint32_t> messages;
        std::shared_ptr<Event> event;
    } applet;
    struct ServiceState {
        enum class Kind { Demo, Manager, Files, File, Display, Input, Apm, ApmSession,
            AppletRoot, AppletProxy, AppletFunctions, LibraryCreator, CommonState, SelfController, WindowController,
            AudioController, DisplayController, DebugFunctions };
        Kind kind = Kind::Demo;
        bool initialized = false;
        std::shared_ptr<const ReadOnlyFiles> files;
        std::shared_ptr<const ReadOnlyFiles::Bytes> file;
    };
    struct Domain {
        struct Object { uint32_t id=0;std::shared_ptr<ServiceState> service; };
        std::array<Object,DomainObjectLimit> objects{};
        uint32_t nextId=2;
    };
    struct Session {
        std::shared_ptr<ServiceState> service;
        std::shared_ptr<Domain> domain;
        bool closed = false;
        Session() : service(std::make_shared<ServiceState>()) {}
        explicit Session(std::shared_ptr<ServiceState> shared) : service(std::move(shared)) {}
    };
    struct Handle {
        uint32_t id = 0; bool writable = false;
        std::shared_ptr<Event> event; bool isThread = false;
        std::shared_ptr<Session> session;
        bool isMainThread = false;
        bool occupied() const { return bool(event) || bool(session); }
    };
    std::array<Handle,128> handles{};
    struct Wait {
        std::array<std::shared_ptr<Event>,64> events{};
        size_t count = 0;
        bool pending = false, timed = false, sleep = false, completed = false;
        uint64_t deadline = 0;
        uint32_t result = Success;
        std::optional<size_t> index;
    };
    std::array<Wait,2> waits{};
    std::shared_ptr<Event> workerExit;
    std::shared_ptr<Event> mainExit;
    uint32_t mainThreadHandle = 0;
    uint32_t nextHandle = 1;
    uint64_t virtualNanoseconds = 0;
    std::shared_ptr<const ReadOnlyFiles> readOnlyFiles;
    std::shared_ptr<const Frame> lastFrame;
    uint64_t presentedFrames = 0;
    uint32_t inputButtons = 0;
    void resolveWaits();
    bool finishWait(size_t index, CPU::Context& context);
    Handle* findHandle(uint32_t id);
    Thread& at(size_t index) { return index == 0 ? thread : worker; }
};
