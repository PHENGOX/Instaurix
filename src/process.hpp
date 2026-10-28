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
#include "memory.hpp"
#include "gpu_channel.hpp"
namespace Ipc { struct QueueSurface; }
namespace Gpu { class AddressSpace; struct VirtualAddress; }

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
    static constexpr size_t ManagedLayerLimit = 64;
    static constexpr size_t NvDescriptorLimit = 32;
    static constexpr size_t NvMapObjectLimit = 64;
    static constexpr size_t GpfifoEntries=0x800,GpfifoLimit=8; // Local reference profile, per NV service.
    size_t managedLayerCount() const { return applet.managedLayers; }
    static constexpr size_t HidSharedSize=0x40000;
    static constexpr uint64_t ReferenceUnixEpoch=946684800; // 2000-01-01 UTC, explicit local reference clock.
    uint64_t appletResourceId() const { return applet.id; }
    bool appletActive() const { return applet.live; }
    bool appletRunningNotified() const { return applet.live && applet.runningNotified; }
    enum class OperationMode : uint8_t { Handheld=0, Docked=1 };
    OperationMode appletOperationMode() const { return applet.operationMode; }
    uint32_t appletPerformanceMode() const { return applet.operationMode==OperationMode::Docked ? 1 : 0; }
    bool setAppletOperationMode(OperationMode mode);
    enum class AppletFocus : uint8_t { InFocus=1, OutOfFocus=2, Background=3 };
    bool appletFocused() const { return applet.focus==AppletFocus::InFocus; }
    AppletFocus appletFocusState() const { return applet.focus; }
    bool appletSuspended() const { return applet.live && applet.suspendBackground && applet.focus==AppletFocus::Background; }
    bool appletSuspendsInBackground() const { return applet.live && applet.suspendBackground; }
    bool appletForegroundRights() const { return applet.live && applet.foregroundRights; }
    bool appletOwnsForeground() const { return appletForegroundRights() && appletFocused(); }
    bool setAppletFocus(bool focused);
    bool setAppletFocusState(AppletFocus focus);
    uint64_t systemTick() const { return (virtualNanoseconds/625)*12+((virtualNanoseconds%625)*12)/625; }
    static constexpr uint64_t HeapBase = 0x200000, HeapCapacity = 0x800000, HeapAlignment = 0x200000;
    static constexpr uint64_t StackSize = 0x10000;
    static constexpr uint32_t Success = 0, InvalidSize = 0xCA01, InvalidAddress = 0xCC01,
        OutOfResource = 0xCE01, OutOfMemory = 0xD001, InvalidPointer = 0xE601;

    bool initialize(Memory& ram, const ProgramLoader::Image& image, bool reference, std::string& error, bool homebrew = false,Gpu::ProfileId gpuProfile=Gpu::ProfileId::MaxwellReference);
    Gpu::ProfileId gpuProfile() const { return selectedGpuProfile; }
    static constexpr uint64_t HomebrewEnvironment = 0xD00000, HomebrewReturn = 0xD01000;
    static constexpr uint64_t AliasBase = HeapBase+HeapCapacity, AliasSize = WorkerStackBase-AliasBase;
    uint32_t mainHandle() const { return mainThreadHandle; }
    Action dispatch(uint16_t id, CPU& cpu, Bus& bus);
    // Synchronous reference GPU access; endpoint and descriptor ownership remain explicit.
    bool readGpuMemory(Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,std::span<uint8_t> output) const;
    bool writeGpuMemory(Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,std::span<const uint8_t> input) const;
    void checkpoint(const CPU& cpu, State next);
    State state() const { return processState; }
    const Thread& mainThread() const { return thread; }
    uint64_t heapSize() const { return currentHeapSize; }
    bool referenceEnabled() const { return enabled; }
    void setReadOnlyFiles(std::shared_ptr<const ReadOnlyFiles> files) { readOnlyFiles = std::move(files); }
    const std::shared_ptr<const Frame>& frame() const { return lastFrame; }
    uint64_t frameCount() const { return presentedFrames; }
    // Host diagnostic only; no guest query command is invented for notifier state.
    std::optional<bool> gpuErrorNotifierEnabled(uint32_t endpoint,uint32_t descriptor) const;
    std::optional<uint32_t> gpuChannelPriority(uint32_t endpoint,uint32_t descriptor) const; // 0 means unconfigured.
    struct GpuChannelExecution {
        uint32_t reference=0;bool referenceWritten=false;uint64_t submissions=0;
        bool operator==(const GpuChannelExecution&) const=default;
    };
    // Synchronous local interpreter progress only; not a native syncpoint or guest API.
    std::optional<GpuChannelExecution> gpuChannelExecution(uint32_t endpoint,uint32_t descriptor) const;
    std::optional<Gpu::ChannelState> gpuChannelState(uint32_t endpoint,uint32_t descriptor) const;
    bool setButtons(uint32_t buttons) {
        if(buttons & ~InputState::Mask) return false;
        inputButtons=buttons;return true;
    }
private:
    std::optional<uint64_t> resolveGpuMemory(const Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,size_t bytes,uint8_t permission) const;
    State processState = State::Ready;
    Gpu::ProfileId selectedGpuProfile=Gpu::ProfileId::MaxwellReference;
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
    struct Event { bool signaled = false, serviceManaged = false, gpuError = false; };
    struct SharedMemory { Memory::SharedPages pages;std::optional<uint64_t> mapping; };
    std::shared_ptr<SharedMemory> hidMemory;
    struct ServiceState;
    struct ManagedLayer {
        bool opened=false,connected=false;
        std::array<uint32_t,64> slots{}; // Process-local Nvmap IDs.
        std::array<std::shared_ptr<const Ipc::QueueSurface>,64> surfaces{};
        uint64_t acquired=0;
        uint32_t nextSlot=0; // Local fair reuse policy for block-linear CPU buffers.
        uint32_t weak=0,strong=0;
        std::weak_ptr<ServiceState> owner;
        std::shared_ptr<Event> available;
    };
    std::array<ManagedLayer,ManagedLayerLimit> managedLayers{};
    struct AppletState {
        uint64_t id=0;
        size_t managedLayers=0;
        bool live=false, foregroundRights=false, suspendBackground=false, runningNotified=false;
        bool operationNotifications=false, performanceNotifications=false;
        OperationMode operationMode=OperationMode::Handheld;
        AppletFocus focus=AppletFocus::InFocus;
        std::deque<uint32_t> messages;
        std::shared_ptr<Event> event;
    } applet;
    struct TransferMemory { uint64_t address=0,size=0; };
    std::array<std::shared_ptr<TransferMemory>,128> transfers{};
    struct ServiceState {
        enum class Kind { Demo, Manager, Files, File, Display, Input, Apm, ApmSession,
            AppletRoot, AppletProxy, AppletFunctions, LibraryCreator, CommonState, SelfController, WindowController,
            AudioController, DisplayController, DebugFunctions, Hid, HidResource, TimeRoot, TimeClock, FsProxy, FsSdRoot, FsNativeFile, ViRoot, ViApplication, ViRelay, NvDriver };
        Kind kind = Kind::Demo;
        bool initialized = false;
        bool nvClientRegistered=false;
        std::shared_ptr<TransferMemory> nvMemory;
        std::array<uint32_t,NvDescriptorLimit> nvDescriptors{};
        enum class NvDevice : uint8_t { None, Map, Control, AddressSpace, Channel };
        std::array<NvDevice,NvDescriptorLimit> nvDevices{};
        std::array<uint32_t,NvDescriptorLimit> nvChannelMapFds{};
        std::array<uint32_t,NvDescriptorLimit> nvChannelAddressFds{};
        struct Gpfifo {
            std::array<uint64_t,GpfifoEntries> entries{};uint32_t fence=0;
            Gpu::ChannelState channel;uint64_t submissions=0;
        };
        std::array<std::shared_ptr<const Gpfifo>,NvDescriptorLimit> nvGpfifos{};
        struct GraphicsContext { uint32_t classNumber=0;uint64_t id=0; };
        std::array<GraphicsContext,NvDescriptorLimit> nvGraphicsContexts{};
        std::array<std::shared_ptr<Event>,NvDescriptorLimit> nvErrorEvents{};
        std::array<bool,NvDescriptorLimit> nvNotifierEnabled{};
        std::array<uint32_t,NvDescriptorLimit> nvPriorities{};
        struct NvAddressSpace { bool initialized=false;uint32_t pageSize=0;std::shared_ptr<const Gpu::AddressSpace> reservations; };
        std::array<NvAddressSpace,NvDescriptorLimit> nvAddressSpaces{};
        struct MapObject { uint32_t id=0,fd=0,size=0; uint64_t backing=0; uint32_t flags=0,alignment=0; };
        std::array<MapObject,NvMapObjectLimit> nvObjects{};
        uint32_t displayReferences=0;
        std::shared_ptr<const ReadOnlyFiles> files;
        std::shared_ptr<const ReadOnlyFiles::Bytes> file;
        std::shared_ptr<SharedMemory> sharedMemory;
        uint8_t timeRole=0;
    };
    const ServiceState::Gpfifo* findGpuFifo(uint32_t endpoint,uint32_t descriptor) const;
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
        std::shared_ptr<SharedMemory> sharedMemory;
        std::shared_ptr<TransferMemory> transferMemory;
        bool occupied() const { return bool(event) || bool(session) || bool(sharedMemory) || bool(transferMemory); }
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
    uint32_t nextNvDescriptor=1;
    uint32_t nextNvMapObject=1;
    uint32_t nextGpuFence=1; // Local allocation identity; not a native syncpoint or completion signal.
    uint64_t nextGpuContext=1; // Local graphics-object identity, distinct from Nvmap objects and kernel handles.
    uint64_t virtualNanoseconds = 0;
    std::shared_ptr<const ReadOnlyFiles> readOnlyFiles;
    std::shared_ptr<const Frame> lastFrame;
    uint64_t presentedFrames = 0;
    uint32_t inputButtons = 0;
    Memory* transferMemoryOwner=nullptr; // Bound on first live SVC, never to staged load memory.
    bool collectTransferMemory(Memory& ram,bool force=false);
    void resolveWaits();
    bool finishWait(size_t index, CPU::Context& context);
    Handle* findHandle(uint32_t id);
    bool overlapsNvBacking(uint64_t address,uint64_t size) const;
    bool nvMapRegistered(uint32_t id) const;
    Thread& at(size_t index) { return index == 0 ? thread : worker; }
};
