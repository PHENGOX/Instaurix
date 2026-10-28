#include "process.hpp"
#include "memory.hpp"
#include "ipc.hpp"
#include "ipc_binder.hpp"
#include "ipc_nv.hpp"
#include "gpu_va.hpp"
#include "gpu_submission.hpp"
#include "gpu_packets.hpp"
#include <array>
#include <algorithm>
#include <string_view>
#include <atomic>

namespace {
std::atomic<uint64_t> nextAppletResource{1};
uint64_t allocateAppletResource() {
    auto id=nextAppletResource.load(std::memory_order_relaxed);
    while(id!=UINT64_MAX) if(nextAppletResource.compare_exchange_weak(id,id+1,std::memory_order_relaxed)) return id;
    return 0;
}
// The original VI OpenLayer client sends one PID marker and one normal B buffer.
// PID placeholder bytes are not an authenticated process identity.
uint32_t parseViOpenLayer(const Ipc::Buffer& request,Bus& bus,uint64_t ipcAddress,Ipc::Layout& layout) {
    auto read=[&](size_t at,size_t size=4){uint64_t v=0;for(size_t i=0;i<size;++i)v|=uint64_t(request[at+i])<<(8*i);return v;};
    if(read(0)!=0x01000004 || (read(4)&~uint64_t(0x800003FF)) || !(read(4)&0x80000000U) || read(8)!=1)
        return Ipc::UnsupportedFormat;
    const auto words=read(4)&0x3FF;
    if(words<8 || words>(Ipc::BufferSize-32)/4)return Ipc::InvalidMessage;
    if(read(32)!=0x49434653)return Ipc::InvalidMessage;
    if(read(36) || read(44) || read(40)!=2020)return Ipc::UnsupportedFormat;
    const auto flags=read(28);
    if(flags&3)return Ipc::UnsupportedFormat;
    const auto size=read(20)|((flags>>24&15)<<32);
    const auto address=read(24)|((flags>>28&15)<<32)|((flags>>2&0x3FFFFF)<<36);
    if(size>Ipc::MaxTransfer)return Ipc::TransferLimit;
    if(size && (!bus.isRamRange(address,size) || !bus.getMemory().canAccess(address,size,Memory::Write)))return Ipc::InvalidBuffer;
    if(size && (address>=ipcAddress ? address-ipcAddress<Ipc::BufferSize : ipcAddress-address<size))return Ipc::InvalidBuffer;
    layout={2020,48,size_t(words*4-32),0,1,0,0,address,size};
    return 0;
}
}

bool Process::initialize(Memory& ram, const ProgramLoader::Image& image, bool reference, std::string& error, bool homebrew,Gpu::ProfileId gpuProfile)
{
    error.clear();
    if(!Gpu::findProfile(gpuProfile)){error="Unknown GPU profile";return false;}
    if(homebrew && (!reference || image.format!=ProgramLoader::Format::Nro))
    { error="Homebrew profile requires NRO and reference HLE";return false; }
    ProgramLoader::RuntimeLayout layout;
    if(homebrew && !ProgramLoader::validateNroRuntime(ram,image,error,&layout)) return false;
    if(image.requiresNroRuntime && !homebrew)
    { error="NRO MOD0: homebrew runtime (libnx environment and relocations) unsupported";return false; }
    if (reference && ((image.format != ProgramLoader::Format::Elf64 && image.format != ProgramLoader::Format::Nro) || ram.size() < HeapBase + HeapCapacity + StackSize + 2 * Memory::PAGE_SIZE))
    { error = "Reference HLE requires static ELF/NRO and space for heap/stack/TLR"; return false; }
    const uint64_t tlsPage = ram.size() >= StackSize + 2 * Memory::PAGE_SIZE ? ram.size() - StackSize - 2 * Memory::PAGE_SIZE : 0;
    if (reference)
        for (const auto& region : image.regions)
            if ((region.begin < HeapBase + HeapCapacity && region.end > HeapBase) ||
                (region.begin < tlsPage + Memory::PAGE_SIZE && region.end > tlsPage) ||
                (region.begin < WorkerTlsPage + Memory::PAGE_SIZE && region.end > WorkerStackBase-Memory::PAGE_SIZE) ||
                (homebrew && region.begin<HomebrewReturn+Memory::PAGE_SIZE && region.end>HomebrewEnvironment) ||
                (homebrew && region.begin<AliasBase+AliasSize && region.end>AliasBase))
            { error = "Reference HLE segment overlaps the heap, a TLR or the second thread stack"; return false; }
    transferMemoryOwner=nullptr;
    transfers={};
    nextNvDescriptor=1;
    nextNvMapObject=1;
    nextGpuFence=1;
    nextGpuContext=1;
    types.assign((ram.size() + Memory::PAGE_SIZE - 1) / Memory::PAGE_SIZE,0);
    memoryAttributes.assign(types.size(),0);runtimeLayout=layout;
    if (image.format == ProgramLoader::Format::Raw) std::fill(types.begin(),types.end(),2); // local raw memory
    else
    {
        for (const auto& region : image.regions)
            for (uint64_t address = region.begin; address < region.end; address += Memory::PAGE_SIZE)
                types[static_cast<size_t>(address / Memory::PAGE_SIZE)] = (region.permission & Memory::Write) ? 4 : 3;
        for (uint64_t address = ram.size()-StackSize; address < ram.size(); address += Memory::PAGE_SIZE)
            types[static_cast<size_t>(address / Memory::PAGE_SIZE)] = 11;
    }
    if (reference)
    {
        if (!ram.setPermissions(tlsPage,Memory::PAGE_SIZE,Memory::Read | Memory::Write))
        { error = ram.getLastError(); return false; }
        types[static_cast<size_t>(tlsPage / Memory::PAGE_SIZE)] = 12;
        if (!ram.setPermissions(WorkerStackBase,StackSize,Memory::Read | Memory::Write) ||
            !ram.setPermissions(WorkerTlsPage,Memory::PAGE_SIZE,Memory::Read | Memory::Write))
        { error = ram.getLastError(); return false; }
        for (uint64_t page = WorkerStackBase; page < WorkerStackTop; page += Memory::PAGE_SIZE)
            types[static_cast<size_t>(page / Memory::PAGE_SIZE)] = 11;
        types[static_cast<size_t>(WorkerTlsPage / Memory::PAGE_SIZE)] = 12;
    }
    enabled = reference;
    selectedGpuProfile=gpuProfile;
    homebrewProfile = homebrew;
    thread = {};
    thread.localRegion = reference ? tlsPage + (homebrew ? 0 : 0x200) : 0;
    processState = State::Ready;
    currentHeapSize = 0;
    handles = {}; waits = {}; nextHandle = 1; virtualNanoseconds = 0;
    mainExit.reset();mainThreadHandle=0;
    worker = {}; worker.id = 2; worker.localRegion = WorkerTlsPage + (homebrew ? 0 : 0x200);
    active = 0; started = {true,false}; workerCreated = false; workerExit.reset();
    readOnlyFiles.reset();
    lastFrame.reset(); presentedFrames=0;
    inputButtons=0;
    applet={};
    managedLayers={};
    hidMemory.reset();
    if(homebrew) {
        applet.event=std::make_shared<Event>();applet.event->serviceManaged=true;
        mainExit=std::make_shared<Event>();
        auto& handle=handles[0];handle.id=nextHandle++;handle.event=mainExit;handle.isThread=true;handle.isMainThread=true;
        mainThreadHandle=handle.id;
        if(!ram.setPermissions(HeapBase,HeapAlignment,Memory::Read|Memory::Write) ||
           !ram.setPermissions(HomebrewEnvironment,Memory::PAGE_SIZE,Memory::Read|Memory::Write) ||
           !ram.setPermissions(HomebrewReturn,Memory::PAGE_SIZE,Memory::Read|Memory::Write))
        { error="Homebrew profile: environment mapping failed";return false; }
        currentHeapSize=HeapAlignment;
        for(uint64_t at=HeapBase;at<HeapBase+currentHeapSize;at+=Memory::PAGE_SIZE) types[size_t(at/Memory::PAGE_SIZE)]=5;
        // ConfigEntry: Key(u32), Flags(u32), Value[2](u64). The final zero entry terminates the list.
        std::array<uint8_t,72> config{};
        auto put=[&](size_t at,uint64_t value,size_t size) { for(size_t i=0;i<size;++i) config[at+i]=uint8_t(value>>(8*i)); };
        put(0,1,4);put(8,mainThreadHandle,8);
        put(24,3,4);put(32,HeapBase,8);put(40,currentHeapSize,8);
        const std::array<uint8_t,8> trampoline{0x21,0x00,0x1E,0xD4,0x00,0x00,0x00,0x14}; // svc #F001; b .
        if(!ram.writeBytes(HomebrewEnvironment,config.data(),config.size()) ||
           !ram.writeBytes(HomebrewReturn,trampoline.data(),trampoline.size()) ||
           !ram.setPermissions(HomebrewEnvironment,Memory::PAGE_SIZE,Memory::Read) ||
           !ram.setPermissions(HomebrewReturn,Memory::PAGE_SIZE,Memory::Read|Memory::Execute))
        { error="Homebrew profile: environment setup failed";return false; }
        types[size_t(HomebrewEnvironment/Memory::PAGE_SIZE)]=3;types[size_t(HomebrewReturn/Memory::PAGE_SIZE)]=3;
        applet.id=allocateAppletResource();
        if(!applet.id) { error="Applet resource IDs exhausted";return false; }
        applet.live=true;
    }
    return true;
}

bool Process::setAppletFocus(bool focused)
{
    return setAppletFocusState(focused ? AppletFocus::InFocus : AppletFocus::OutOfFocus);
}

bool Process::setAppletOperationMode(OperationMode mode)
{
    if(!applet.live || (mode!=OperationMode::Handheld && mode!=OperationMode::Docked)) return false;
    if(applet.operationMode==mode) return true;
    const size_t count=size_t(applet.operationNotifications)+size_t(applet.performanceNotifications);
    if(applet.messages.size()>AppletMessageLimit-count) return false;
    if(count) {
        try {
            auto messages=applet.messages;
            if(applet.operationNotifications) messages.push_back(30);
            if(applet.performanceNotifications) messages.push_back(31);
            applet.messages.swap(messages);
        } catch(const std::bad_alloc&) { return false; }
    }
    applet.operationMode=mode;
    if(count) { applet.event->signaled=true;resolveWaits(); }
    return true;
}

bool Process::setAppletFocusState(AppletFocus focus)
{
    if(!applet.live) return false;
    if(focus!=AppletFocus::InFocus && focus!=AppletFocus::OutOfFocus && focus!=AppletFocus::Background) return false;
    if(applet.focus==focus) return true;
    if(applet.messages.size()==AppletMessageLimit) return false;
    try { applet.messages.push_back(15); }
    catch(const std::bad_alloc&) { return false; }
    applet.focus=focus;applet.event->signaled=true;resolveWaits();return true;
}

std::optional<uint64_t> Process::resolveGpuMemory(const Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,size_t bytes,uint8_t permission) const
{
    if(!enabled || !applet.live || processState==State::Exited || processState==State::Faulted || !bytes || transferMemoryOwner!=&bus.getMemory())return {};
    const auto handle=std::find_if(handles.begin(),handles.end(),[&](const auto& h){return h.id==endpoint && h.occupied();});
    if(handle==handles.end() || !handle->session || handle->session->closed || handle->session->domain)return {};
    const auto& service=handle->session->service;
    if(!service || service->kind!=ServiceState::Kind::NvDriver || !service->initialized || !service->nvClientRegistered)return {};
    const auto fd=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),descriptor);
    if(!descriptor || fd==service->nvDescriptors.end())return {};
    const auto index=size_t(fd-service->nvDescriptors.begin());
    const auto& space=service->nvAddressSpaces[index];
    if(service->nvDevices[index]!=ServiceState::NvDevice::AddressSpace || !space.initialized || !space.reservations)return {};
    Gpu::AddressSpace::Translation translation;
    if(!space.reservations->translate(address,bytes,translation))return {};
    const auto object=std::find_if(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& o){return o.id==translation.object;});
    if(object==service->nvObjects.end() || !object->backing || object->flags!=1 || translation.offset>object->size || bytes>object->size-translation.offset)return {};
    if(translation.offset>UINT64_MAX-object->backing)return {};
    const auto cpuAddress=object->backing+translation.offset;
    if(!bus.isRamRange(cpuAddress,bytes) || !bus.getMemory().canAccess(cpuAddress,bytes,permission))return {};
    return cpuAddress;
}

bool Process::readGpuMemory(Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,std::span<uint8_t> output) const
{
    const auto source=resolveGpuMemory(bus,endpoint,descriptor,address,output.size(),Memory::Read);
    if(!source)return false;
    for(size_t i=0;i<output.size();++i)output[i]=bus.getMemory().read8(*source+i);
    return true;
}

bool Process::writeGpuMemory(Bus& bus,uint32_t endpoint,uint32_t descriptor,Gpu::VirtualAddress address,std::span<const uint8_t> input) const
{
    const auto destination=resolveGpuMemory(bus,endpoint,descriptor,address,input.size(),Memory::Write);
    return destination && bus.getMemory().writeBytes(*destination,input.data(),input.size());
}

std::optional<bool> Process::gpuErrorNotifierEnabled(uint32_t endpoint,uint32_t descriptor) const
{
    if(!enabled || !homebrewProfile || !applet.live || processState==State::Exited || processState==State::Faulted)return {};
    const auto handle=std::find_if(handles.begin(),handles.end(),[&](const auto& h){return h.id==endpoint && h.occupied();});
    if(handle==handles.end() || !handle->session || handle->session->closed || handle->session->domain)return {};
    const auto& service=handle->session->service;
    if(!service || service->kind!=ServiceState::Kind::NvDriver || !service->initialized || !service->nvClientRegistered || !descriptor)return {};
    const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),descriptor);
    if(found==service->nvDescriptors.end())return {};
    const auto slot=size_t(found-service->nvDescriptors.begin());
    if(service->nvDevices[slot]!=ServiceState::NvDevice::Channel || !service->nvGraphicsContexts[slot].id || !service->nvErrorEvents[slot])return {};
    return service->nvNotifierEnabled[slot];
}

std::optional<uint32_t> Process::gpuChannelPriority(uint32_t endpoint,uint32_t descriptor) const
{
    if(!enabled || !homebrewProfile || !applet.live || processState==State::Exited || processState==State::Faulted)return {};
    const auto handle=std::find_if(handles.begin(),handles.end(),[&](const auto& h){return h.id==endpoint && h.occupied();});
    if(handle==handles.end() || !handle->session || handle->session->closed || handle->session->domain)return {};
    const auto& service=handle->session->service;
    if(!service || service->kind!=ServiceState::Kind::NvDriver || !service->initialized || !service->nvClientRegistered || !descriptor)return {};
    const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),descriptor);
    if(found==service->nvDescriptors.end())return {};
    const auto slot=size_t(found-service->nvDescriptors.begin());
    if(service->nvDevices[slot]!=ServiceState::NvDevice::Channel || !service->nvChannelMapFds[slot])return {};
    return service->nvPriorities[slot];
}

const Process::ServiceState::Gpfifo* Process::findGpuFifo(uint32_t endpoint,uint32_t descriptor) const
{
    if(!enabled || !homebrewProfile || !applet.live || processState==State::Exited || processState==State::Faulted)return {};
    const auto handle=std::find_if(handles.begin(),handles.end(),[&](const auto& h){return h.id==endpoint && h.occupied();});
    if(handle==handles.end() || !handle->session || handle->session->closed || handle->session->domain)return {};
    const auto& service=handle->session->service;
    if(!service || service->kind!=ServiceState::Kind::NvDriver || !service->initialized || !service->nvClientRegistered || !descriptor)return {};
    const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),descriptor);
    if(found==service->nvDescriptors.end())return {};
    const auto slot=size_t(found-service->nvDescriptors.begin());
    if(service->nvDevices[slot]!=ServiceState::NvDevice::Channel || !service->nvGpfifos[slot])return {};
    return service->nvGpfifos[slot].get();
}
std::optional<Process::GpuChannelExecution> Process::gpuChannelExecution(uint32_t endpoint,uint32_t descriptor) const
{
    const auto fifo=findGpuFifo(endpoint,descriptor);if(!fifo)return {};
    return GpuChannelExecution{fifo->channel.reference.value,fifo->channel.reference.written,fifo->submissions};
}
std::optional<Gpu::ChannelState> Process::gpuChannelState(uint32_t endpoint,uint32_t descriptor) const
{
    const auto fifo=findGpuFifo(endpoint,descriptor);if(!fifo)return {};
    return fifo->channel;
}

bool Process::nvMapRegistered(uint32_t id) const
{
    if(!id)return false;
    for(const auto& layer:managedLayers)for(auto object:layer.slots)if(object==id)return true;
    for(const auto& handle:handles)if(handle.session && handle.session->service)
        for(const auto& space:handle.session->service->nvAddressSpaces)if(space.reservations && space.reservations->references(id))return true;
    return false;
}

bool Process::overlapsNvBacking(uint64_t address,uint64_t size) const
{
    if(!size)return false;
    for(const auto& handle:handles)if(handle.session && handle.session->service){
        for(const auto& object:handle.session->service->nvObjects)
            if(object.backing && (address>=object.backing?address-object.backing<object.size:object.backing-address<size))return true;
    }
    return false;
}
Process::Handle* Process::findHandle(uint32_t id)
{
    for (auto& handle : handles) if (handle.id == id && handle.occupied()) return &handle;
    return nullptr;
}

uint32_t Process::signalEvent(uint32_t id)
{
    auto* handle = findHandle(id);
    if (!handle || !handle->event || !handle->writable || handle->isThread) return InvalidHandle;
    handle->event->signaled = true;
    resolveWaits();
    return Success;
}

bool Process::resumeWait(CPU& cpu)
{
    resolveWaits();
    auto context = cpu.captureContext();
    if (finishWait(active,context)) cpu.restoreContext(context);
    return !waits[active].pending;
}

void Process::resolveWaits()
{
    for (auto& wait : waits)
    {
        if (!wait.pending || wait.completed) continue;
        for (size_t i = 0; i < wait.count; ++i) if (wait.events[i]->signaled)
        { wait.completed = true; wait.result = Success; wait.index = i; break; }
        if (!wait.completed && wait.timed && virtualNanoseconds >= wait.deadline)
        { wait.completed = true; wait.result = wait.sleep ? Success : TimedOut; }
        if (wait.completed) wait.events = {}; // Completion is latched, independent of later clears.
    }
}

bool Process::finishWait(size_t index, CPU::Context& context)
{
    auto& wait = waits[index];
    if (!wait.pending || !wait.completed) return false;
    if (!wait.sleep)
    {
        context.registers[0] = wait.result;
        if (wait.index) context.registers[1] = *wait.index;
    }
    wait = {}; at(index).state = State::Ready;
    return true;
}

bool Process::advanceTime(uint64_t nanoseconds)
{
    if (nanoseconds > UINT64_MAX - virtualNanoseconds) return false;
    // Host call order settles signal/timeout races; never consult wall-clock time.
    resolveWaits();
    virtualNanoseconds += nanoseconds;
    resolveWaits();
    return true;
}

std::optional<uint64_t> Process::nextWakeDelay() const
{
    if(appletSuspended()) return std::nullopt;
    std::optional<uint64_t> delay;
    for (const auto& wait : waits)
    {
        if (!wait.pending) continue;
        if (wait.completed) return 0;
        if (wait.timed)
        {
            const auto remaining = wait.deadline > virtualNanoseconds ? wait.deadline-virtualNanoseconds : 0;
            if (!delay || remaining < *delay) delay = remaining;
        }
    }
    return delay;
}

bool Process::allThreadsExited() const
{
    return thread.state == State::Exited && (!workerCreated || worker.state == State::Exited);
}

bool Process::schedule(CPU& cpu, bool rotate)
{
    at(active).context = cpu.captureContext();
    if(appletSuspended()) return false;
    resolveWaits();
    bool activeWoke = false;
    for (size_t index = 0; index < waits.size(); ++index)
    {
        if (finishWait(index,at(index).context) && index == active) activeWoke = true;
    }
    auto runnable = [&](size_t index) {
        return started[index] && !waits[index].pending && at(index).state != State::Exited;
    };
    size_t chosen = active;
    if ((rotate || !runnable(active)) && runnable(1-active)) chosen = 1-active;
    else if (!runnable(active)) return false;
    if (chosen != active || activeWoke)
    {
        if (at(active).state == State::Running) at(active).state = State::Ready;
        active = chosen;
        // Context restoration also clears the CPU's exclusive reservation.
        cpu.restoreContext(at(active).context);
    }
    at(active).state = State::Running;
    return true;
}

bool Process::collectTransferMemory(Memory& ram,bool force)
{
    for(auto& transfer:transfers)if(transfer && (force || transfer.use_count()==1)){
        const auto memory=*transfer;
        if(!ram.setPermissions(memory.address,size_t(memory.size),Memory::Read|Memory::Write))return false;
        for(size_t page=size_t(memory.address/Memory::PAGE_SIZE);page<size_t((memory.address+memory.size)/Memory::PAGE_SIZE);++page)memoryAttributes[page]=0;
        transfer.reset();
    }
    return true;
}
void Process::checkpoint(const CPU& cpu, State next)
{
    processState = next;
    at(active).context = cpu.captureContext();
    if (next == State::Exited || next == State::Faulted)
    {
        if(transferMemoryOwner){
            for(auto& handle:handles)if(handle.transferMemory || (handle.event && handle.event->gpuError) || (handle.session && handle.session->service->kind==ServiceState::Kind::NvDriver))handle={};
            collectTransferMemory(*transferMemoryOwner,true);
        }
        applet.live=false;applet.foregroundRights=false;applet.managedLayers=0;managedLayers={};applet.messages.clear();if(applet.event) applet.event->signaled=false;
        thread.state = next;
        if (workerCreated) worker.state = next;
        waits = {};
        if (workerExit) workerExit->signaled = true;
        if (mainExit) mainExit->signaled = true;
    }
    else if (at(active).state != State::Exited && !waits[active].pending)
        at(active).state = next;
}

Process::Action Process::dispatch(uint16_t id, CPU& cpu, Bus& bus)
{
    if (!enabled) return Action::Unsupported;
    Memory& ram = bus.getMemory();
    auto result = [&](uint32_t value) { cpu.writeRegister(0,value); return Action::Continue; };
    switch (id)
    {
    case 0x29: // GetInfo: bounded local process regions, not a console address-space model.
    {
        if(!homebrewProfile) return Action::Unsupported;
        if(cpu.readRegister(1)==11) { // Reproducible test entropy for original libnx's PRNG, not physical TRNG output.
            if(cpu.readRegister(2)!=0) return result(InvalidHandle);
            if(cpu.readRegister(3)>=4) return result(InvalidSize);
            constexpr std::array<uint64_t,4> entropy{0x243F6A8885A308D3ULL,0x13198A2E03707344ULL,0xA4093822299F31D0ULL,0x082EFA98EC4E6C89ULL};
            cpu.writeRegister(1,entropy[size_t(cpu.readRegister(3))]);return result(Success);
        }
        if(static_cast<uint32_t>(cpu.readRegister(2))!=0xFFFF8001U) return result(InvalidHandle);
        if(cpu.readRegister(3)!=0) return result(InvalidSize);
        uint64_t value=0;
        switch(cpu.readRegister(1)) {
            case 2:value=AliasBase;break; case 3:value=AliasSize;break;
            case 4:value=HeapBase;break; case 5:value=HeapCapacity;break;
            case 12:value=0x10000;break; case 13:value=ram.size()-0x10000;break;
            case 14:value=ram.size()-StackSize;break; case 15:value=StackSize;break;
            case 28:value=0;break;
            default:return Action::Unsupported;
        }
        cpu.writeRegister(1,value);return result(Success);
    }
    case 0x02: // Bounded homebrew RELRO protection, never grants execute permission.
    case 0x03: // Lock permissions on validated, already read-only RELRO pages.
    {
        if(!runtimeLayout.relroBegin) return Action::Unsupported;
        const auto address=cpu.readRegister(0), size=cpu.readRegister(1);
        if(address%Memory::PAGE_SIZE) return result(InvalidAddress);
        if(!size || size%Memory::PAGE_SIZE) return result(InvalidSize);
        if(address<runtimeLayout.relroBegin || address>=runtimeLayout.relroEnd || size>runtimeLayout.relroEnd-address)
            return result(InvalidAddress);
        const auto first=size_t(address/Memory::PAGE_SIZE), end=first+size_t(size/Memory::PAGE_SIZE);
        const auto value=cpu.readRegister(2), flags=cpu.readRegister(3);
        if(id==2) {
            if(value!=Memory::None && value!=Memory::Read && value!=(Memory::Read|Memory::Write)) return result(InvalidState);
            for(size_t page=first;page<end;++page) if(memoryAttributes[page]&16) return result(InvalidState);
            if(!ram.setPermissions(address,size_t(size),uint8_t(value))) return result(InvalidAddress);
        } else {
            if(value!=16 || flags!=16) return result(InvalidState);
            for(size_t page=first;page<end;++page) if(ram.permissionsAt(page*Memory::PAGE_SIZE)!=Memory::Read)
                return result(InvalidState);
            for(size_t page=first;page<end;++page) memoryAttributes[page]|=16;
        }
        return result(Success);
    }
    case 0x1F: // ConnectToNamedPort: bounded NUL-terminated name at X1.
    {
        const auto address = cpu.readRegister(1);
        std::array<char,12> name{};
        size_t length = 0;
        for (; length < name.size(); ++length)
        {
            if (!bus.isRamRange(address,length+1) || !ram.canAccess(address,length+1,Memory::Read))
                return result(InvalidPointer);
            name[length] = char(ram.read8(address+length));
            if (!name[length]) break;
        }
        if (length == name.size()) return result(InvalidSize);
        const auto port = std::string_view(name.data(),length);
        if (port != "emu:demo" && port != "sm:") return result(Ipc::PortNotFound);
        Handle* slot = nullptr;
        for (auto& handle : handles) if (!handle.occupied()) { slot = &handle; break; }
        if (!slot || nextHandle >= 0xFFFF8000U) return result(OutOfHandles);
        std::shared_ptr<Session> session;
        try { session = std::make_shared<Session>(); }
        catch (const std::bad_alloc&) { return result(OutOfResource); }
        if (port == "sm:") session->service->kind = ServiceState::Kind::Manager;
        *slot = {}; slot->id = nextHandle++; slot->session = std::move(session);
        cpu.writeRegister(1,slot->id);
        return result(Success);
    }
    case 0x21: // SendSyncRequest uses the current thread's canonical IPC region.
    {
        auto* handle = findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if (!handle || !handle->session) return result(InvalidHandle);
        if (handle->session->closed) return result(Ipc::SessionClosed);
        const auto address = at(active).localRegion;
        if (!bus.isRamRange(address,Ipc::BufferSize) ||
            !ram.canAccess(address,Ipc::BufferSize,Memory::Read | Memory::Write)) return result(InvalidPointer);
        Ipc::Buffer request{}, response{};
        for (size_t i=0;i<request.size();++i) request[i] = ram.read8(address+i);
        Ipc::Transfer transfer;
        Ipc::ManagerRequest manager;
        Ipc::ControlRequest control;
        Ipc::FileRequest fileRequest;
        std::shared_ptr<const Frame> pendingFrame;
        Handle* pendingSlot = nullptr;
        std::shared_ptr<Session> pendingSession;
        std::shared_ptr<Domain> pendingDomain, objectDomain;
        Domain::Object* pendingObject = nullptr;
        std::shared_ptr<ServiceState> pendingObjectService;
        std::shared_ptr<const ReadOnlyFiles::Bytes> pendingNativeFile;
        std::shared_ptr<Event> pendingEvent;
        std::shared_ptr<SharedMemory> pendingSharedMemory,pendingHidMemory;
        bool consumeAppletMessage=false;
        bool acquireForeground=false;
        bool notifyRunning=false;
        bool createManagedLayer=false;
        std::shared_ptr<ServiceState> pendingFsAttach,pendingViState;
        uint32_t pendingViReferences=0;
        std::shared_ptr<ServiceState> pendingNvState,pendingNvClient;
        std::shared_ptr<TransferMemory> pendingNvMemory;
        std::shared_ptr<ServiceState> pendingNvDescriptorState;
        size_t pendingNvDescriptorSlot=0;uint32_t pendingNvDescriptor=0;auto pendingNvDevice=ServiceState::NvDevice::None;
        std::shared_ptr<ServiceState> pendingNvAddressState;size_t pendingNvAddressSlot=0;uint32_t pendingNvPageSize=0;
        std::shared_ptr<ServiceState> pendingNvVaState;size_t pendingNvVaSlot=0;std::shared_ptr<Gpu::AddressSpace> pendingNvVa;
        uint32_t pendingNvRetiredFd=0;
        std::shared_ptr<ServiceState> pendingNvChannelState;size_t pendingNvChannelSlot=0;uint32_t pendingNvChannelMapFd=0;
        std::shared_ptr<ServiceState> pendingNvBindingState;size_t pendingNvBindingSlot=0;uint32_t pendingNvBindingAddressFd=0;
        std::shared_ptr<ServiceState> pendingGpfifoState;size_t pendingGpfifoSlot=0;std::shared_ptr<ServiceState::Gpfifo> pendingGpfifo;
        std::shared_ptr<ServiceState> pendingGpuExecutionState;size_t pendingGpuExecutionSlot=0;std::shared_ptr<ServiceState::Gpfifo> pendingGpuExecution;
        struct PendingGpuMemory { uint64_t address=0;std::array<uint8_t,4> bytes{}; };
        std::array<PendingGpuMemory,Gpu::ReferenceWriteLimit> pendingGpuMemory{};size_t pendingGpuMemoryCount=0;
        std::vector<uint8_t> pendingGpuCopyBytes;uint64_t pendingGpuCopyDestination=0;
        uint32_t pendingGpuCopyRows=0,pendingGpuCopyWidth=0,pendingGpuCopyPitch=0;
        std::shared_ptr<ServiceState> pendingGraphicsState;size_t pendingGraphicsSlot=0;
        std::shared_ptr<ServiceState> pendingGpuEventState;size_t pendingGpuEventSlot=0;
        std::shared_ptr<ServiceState> pendingNotifierState;size_t pendingNotifierSlot=0;bool pendingNotifierEnabled=false;
        std::shared_ptr<ServiceState> pendingPriorityState;size_t pendingPrioritySlot=0;uint32_t pendingPriority=0;
        std::shared_ptr<ServiceState> pendingNvMapState;size_t pendingNvMapSlot=0;
        ServiceState::MapObject pendingNvMapObject;
        std::optional<std::pair<size_t,ManagedLayer>> pendingLayer;
        std::optional<bool> pendingSuspendBackground;
        std::optional<bool> pendingOperationNotifications, pendingPerformanceNotifications;
        uint32_t status;
        auto prepareSession = [&](bool clone) {
            for (auto& candidate : handles) if (!candidate.occupied()) { pendingSlot = &candidate; break; }
            if (!pendingSlot || nextHandle >= 0xFFFF8000U) return OutOfHandles;
            pendingSession = clone ? std::make_shared<Session>(handle->session->service) : std::make_shared<Session>();
            if(clone) pendingSession->domain=handle->session->domain;
            return Success;
        };
        try {
            const uint16_t type = uint16_t(request[0]) | (uint16_t(request[1]) << 8);
            if (type == 2 || type == 5)
            {
                status = Ipc::parseControl(request,control);
                if (!status)
                {
                    if (control.close)
                    {
                        const auto service=handle->session->service;
                        if(service->kind==ServiceState::Kind::NvDriver && std::any_of(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return nvMapRegistered(object.id);}))return result(InvalidState);
                        handle->session->closed = true;
                        handle->session->domain.reset();
                        return result(Success); // Close has no CMIF reply; CloseHandle releases the slot.
                    }
                    if(control.clone){
                        if(control.taggedClone && (!homebrewProfile || handle->session->service->kind!=ServiceState::Kind::NvDriver))control.result=Ipc::UnknownCommand;
                        else if(control.taggedClone && control.cloneTag!=1)control.result=Ipc::InvalidArguments;
                        else control.result=prepareSession(true);
                    }
                    if(control.convert) {
                        if(!homebrewProfile || (handle->session->service->kind!=ServiceState::Kind::AppletRoot && handle->session->service->kind!=ServiceState::Kind::FsProxy))
                            control.result=Ipc::UnknownCommand;
                        else if(handle->session->domain) control.result=InvalidState;
                        else {
                            pendingDomain=std::make_shared<Domain>();
                            pendingDomain->objects[0]={1,handle->session->service};
                        }
                    }
                    Ipc::managerResponse(response,control.result,pendingSession ? nextHandle : 0);
                    if(pendingDomain) { response[4]=9;response[32]=1; }
                    // No general server pointer-buffer allocation is implemented; this is not the A/B transfer limit.
                    if (control.queryPointerSize && !control.result) response[4] = 9;
                }
            }
            else if(handle->session->domain)
            {
                Ipc::DomainRequest domainRequest;
                status=Ipc::parseDomain(request,domainRequest,handle->session->service->kind==ServiceState::Kind::FsProxy);
                if(!status) {
                    objectDomain=handle->session->domain;
                    Domain::Object* object=nullptr;
                    for(auto& candidate:objectDomain->objects)
                        if(candidate.id==domainRequest.object && candidate.service) { object=&candidate;break; }
                    if((domainRequest.sendStatic || domainRequest.receiveBuffer) && (!object || (object->service->kind!=ServiceState::Kind::FsSdRoot && object->service->kind!=ServiceState::Kind::FsNativeFile)))return result(Ipc::UnsupportedFormat);
                    if(domainRequest.close) {
                        if(!object) return result(Ipc::InvalidObject);
                        *object={};return result(Success); // Object close keeps the session and has no CMIF response body.
                    }
                    uint32_t code=object ? Ipc::UnknownCommand : Ipc::InvalidObject;
                    uint64_t scalar=0;size_t scalarSize=0;
                    bool open=false;
                    auto kind=ServiceState::Kind::Demo;
                    if(object && (object->service->kind==ServiceState::Kind::FsSdRoot || object->service->kind==ServiceState::Kind::FsNativeFile)) {
                        auto readArg=[&](size_t offset,size_t size=8){uint64_t v=0;for(size_t i=0;i<size;++i)v|=uint64_t(request[domainRequest.payload+offset+i])<<(8*i);return v;};
                        if(!applet.live)code=InvalidState;
                        else if(object->service->kind==ServiceState::Kind::FsSdRoot && domainRequest.command==8) {
                            if(domainRequest.sendPid || !domainRequest.sendStatic || domainRequest.receiveBuffer || domainRequest.payloadSize!=4 || domainRequest.sourceSize!=0x301 || readArg(0,4)!=1)code=Ipc::InvalidArguments;
                            else if(!bus.isRamRange(domainRequest.source,domainRequest.sourceSize) || !ram.canAccess(domainRequest.source,size_t(domainRequest.sourceSize),Memory::Read))return result(Ipc::InvalidBuffer);
                            else {
                                std::string path;bool terminated=false;
                                for(size_t i=0;i<domainRequest.sourceSize;++i){const auto c=ram.read8(domainRequest.source+i);if(!c){terminated=true;break;}path+=char(c);}
                                if(!terminated || path.empty() || path[0]!='/' || !ReadOnlyFiles::validName(path.substr(1)))code=Ipc::InvalidPath;
                                else {pendingNativeFile=object->service->files->find(path.substr(1));if(!pendingNativeFile)code=Ipc::FileNotFound;else {open=true;kind=ServiceState::Kind::FsNativeFile;}}
                            }
                        }else if(object->service->kind==ServiceState::Kind::FsNativeFile && domainRequest.command==4) {
                            if(domainRequest.sendPid || domainRequest.sendStatic || domainRequest.receiveBuffer || domainRequest.payloadSize)code=Ipc::InvalidArguments;
                            else {code=Success;scalar=object->service->file->size();scalarSize=8;}
                        }else if(object->service->kind==ServiceState::Kind::FsNativeFile && domainRequest.command==0) {
                            if(domainRequest.sendPid || domainRequest.sendStatic || !domainRequest.receiveBuffer || domainRequest.payloadSize!=24)code=Ipc::InvalidArguments;
                            else {
                                const auto off=readArg(8),count=readArg(16),capacity=domainRequest.destinationSize,dest=domainRequest.destination;
                                if(readArg(0,4) || readArg(4,4) || (off>>63) || count>capacity)code=Ipc::InvalidArguments;
                                else if(capacity>Ipc::MaxTransfer)return result(Ipc::TransferLimit);
                                else if(capacity && (!bus.isRamRange(dest,capacity) || !ram.canAccess(dest,size_t(capacity),Memory::Write) || (dest<address+Ipc::BufferSize && address<dest+capacity)))return result(Ipc::InvalidBuffer);
                                else {
                                    const auto& bytes=*object->service->file;const size_t size=off>=bytes.size()?0:static_cast<size_t>(std::min<uint64_t>(count,bytes.size()-off));
                                    transfer.address=dest;if(size)transfer.bytes.assign(bytes.begin()+size_t(off),bytes.begin()+size_t(off)+size);
                                    code=Success;scalar=size;scalarSize=8;
                                }
                            }
                        }
                    }
                    if(object && object->service->kind==ServiceState::Kind::FsProxy) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.command==1) {
                            uint64_t placeholder=0;
                            if(domainRequest.payloadSize==8) for(size_t i=0;i<8;++i)placeholder|=uint64_t(request[domainRequest.payload+i])<<(8*i);
                            if(!domainRequest.sendPid || domainRequest.copyHandlePresent || domainRequest.payloadSize!=8 || placeholder) code=Ipc::InvalidArguments;
                            else if(object->service->initialized) code=Ipc::AlreadyInitialized;
                            else { code=Success;pendingFsAttach=object->service; }
                        } else if(!object->service->initialized) code=Ipc::NotInitialized;
                        else if(domainRequest.command==18) {
                            if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                            else if(!readOnlyFiles) code=Ipc::FilesDisabled;
                            else { open=true;kind=ServiceState::Kind::FsSdRoot; }
                        }
                    }
                    if(object && object->service->kind==ServiceState::Kind::AppletRoot && domainRequest.command==0) {
                        uint64_t reserved=0;
                        if(domainRequest.payloadSize==8)
                            for(size_t i=0;i<8;++i) reserved|=uint64_t(request[domainRequest.payload+i])<<(8*i);
                        if(!domainRequest.sendPid || domainRequest.copyHandle!=0xFFFF8001U || domainRequest.payloadSize!=8 || reserved)
                            code=Ipc::InvalidArguments;
                        else { open=true;kind=ServiceState::Kind::AppletProxy; }
                    } else if(object && object->service->kind==ServiceState::Kind::AppletProxy) {
                        switch(domainRequest.command) {
                            case 20:kind=ServiceState::Kind::AppletFunctions;open=true;break;
                            case 11:kind=ServiceState::Kind::LibraryCreator;open=true;break;
                            case 0:kind=ServiceState::Kind::CommonState;open=true;break;
                            case 1:kind=ServiceState::Kind::SelfController;open=true;break;
                            case 2:kind=ServiceState::Kind::WindowController;open=true;break;
                            case 3:kind=ServiceState::Kind::AudioController;open=true;break;
                            case 4:kind=ServiceState::Kind::DisplayController;open=true;break;
                            case 1000:kind=ServiceState::Kind::DebugFunctions;open=true;break;
                        }
                        if(open && (domainRequest.sendPid || domainRequest.payloadSize)) { open=false;code=Ipc::InvalidArguments; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::AppletFunctions && domainRequest.command==40) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else { code=Success;scalar=1;scalarSize=1;notifyRunning=true; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::SelfController && domainRequest.command==40) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else if(applet.managedLayers==ManagedLayerLimit) code=OutOfResource;
                        // Local process-owned logical resources; no native window or GPU allocation yet.
                        else { code=Success;scalar=applet.managedLayers+1;scalarSize=8;createManagedLayer=true; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::WindowController && domainRequest.command==1) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else { code=Success;scalar=applet.id;scalarSize=8; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::WindowController && domainRequest.command==10) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else if(!appletFocused()) code=InvalidState;
                        else { code=Success;acquireForeground=true; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::SelfController &&
                       (domainRequest.command==11 || domainRequest.command==12)) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize!=1 || request[domainRequest.payload]>1) code=Ipc::InvalidArguments;
                        else {
                            code=Success;
                            if(domainRequest.command==11) pendingOperationNotifications=request[domainRequest.payload]!=0;
                            else pendingPerformanceNotifications=request[domainRequest.payload]!=0;
                        }
                    }
                    if(object && object->service->kind==ServiceState::Kind::SelfController && domainRequest.command==13) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize!=3) code=Ipc::InvalidArguments;
                        else {
                            const auto at=domainRequest.payload;
                            const auto a=request[at],b=request[at+1],c=request[at+2];
                            if(a>1 || b>1 || c>1) code=Ipc::InvalidArguments;
                            else if(a==0 && b==0 && c==1) { code=Success;pendingSuspendBackground=true; }
                            else if(a==1 && b==1 && c==0) { code=Success;pendingSuspendBackground=false; }
                            else code=Ipc::UnknownCommand;
                        }
                    }
                    if(object && object->service->kind==ServiceState::Kind::CommonState &&
                       (domainRequest.command==0 || domainRequest.command==1 || domainRequest.command==5 || domainRequest.command==6 || domainRequest.command==9)) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else if(domainRequest.command==0) {
                            for(auto& candidate:handles) if(!candidate.occupied()) { pendingSlot=&candidate;break; }
                            if(!pendingSlot || nextHandle>=0xFFFF8000U) code=OutOfHandles;
                            else { code=Success;pendingEvent=applet.event; }
                        } else if(domainRequest.command==5) { code=Success;scalar=static_cast<uint8_t>(applet.operationMode);scalarSize=1; }
                        else if(domainRequest.command==6) { code=Success;scalar=appletPerformanceMode();scalarSize=4; }
                        else if(domainRequest.command==9) { code=Success;scalar=static_cast<uint8_t>(applet.focus);scalarSize=1; }
                        else if(applet.messages.empty()) code=0x680; // AM NoMessages, consumed by the original applet loop.
                        else { code=Success;scalar=applet.messages.front();scalarSize=4;consumeAppletMessage=true; }
                    }
                    if(open) {
                        for(auto& candidate:objectDomain->objects) if(!candidate.service) { pendingObject=&candidate;break; }
                        if(!pendingObject || objectDomain->nextId==UINT32_MAX) { pendingObject=nullptr;code=Ipc::OutOfObjects; }
                        else {
                            pendingObjectService=std::make_shared<ServiceState>();pendingObjectService->kind=kind;code=Success;
                            if(kind==ServiceState::Kind::FsNativeFile)pendingObjectService->file=pendingNativeFile;
                            if(kind==ServiceState::Kind::FsSdRoot) pendingObjectService->files=readOnlyFiles;
                        }
                    }
                    if(scalarSize) Ipc::domainValueResponse(response,code,scalar,scalarSize);
                    else if(pendingEvent) Ipc::domainEventResponse(response,code,nextHandle);
                    else Ipc::domainResponse(response,code,pendingObject ? objectDomain->nextId : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::AppletRoot || handle->session->service->kind==ServiceState::Kind::FsProxy) status=Ipc::UnsupportedFormat;
            else if (handle->session->service->kind == ServiceState::Kind::Manager)
            {
                status = Ipc::parseManager(request,handle->session->service->initialized,manager);
                if (!status)
                {
                    if (manager.openDemo || manager.openFiles || manager.openDisplay || manager.openInput || manager.openApm || manager.openApplet || manager.openHid || manager.openTime || manager.openFsp || manager.openVi || manager.openNv)
                    {
                        if(manager.openFiles && !readOnlyFiles) manager.result=Ipc::FilesDisabled;
                        else if((manager.openApm || manager.openApplet || manager.openHid || manager.openTime || manager.openFsp || manager.openVi || manager.openNv) && !homebrewProfile) manager.result=Ipc::PortNotFound;
                        else {
                            manager.result = prepareSession(false);
                            if(pendingSession && manager.openFiles) {
                                pendingSession->service->kind=ServiceState::Kind::Files;
                                pendingSession->service->files=readOnlyFiles;
                            }
                            if(pendingSession && manager.openDisplay) pendingSession->service->kind=ServiceState::Kind::Display;
                            if(pendingSession && manager.openInput) pendingSession->service->kind=ServiceState::Kind::Input;
                            if(pendingSession && manager.openHid) pendingSession->service->kind=ServiceState::Kind::Hid;
                            if(pendingSession && manager.openTime) pendingSession->service->kind=ServiceState::Kind::TimeRoot;
                            if(pendingSession && manager.openVi) pendingSession->service->kind=ServiceState::Kind::ViRoot;
                            if(pendingSession && manager.openNv) pendingSession->service->kind=ServiceState::Kind::NvDriver;
                            if(pendingSession && manager.openFsp) pendingSession->service->kind=ServiceState::Kind::FsProxy;
                            if(pendingSession && manager.openApm) pendingSession->service->kind=ServiceState::Kind::Apm;
                            if(pendingSession && manager.openApplet) pendingSession->service->kind=ServiceState::Kind::AppletRoot;
                        }
                    }
                    Ipc::managerResponse(response,manager.result,pendingSession ? nextHandle : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::NvDriver) {
                if(request[3]){
                    Ipc::NvIoctl ioctl;const auto parsed=Ipc::parseNvIoctl(request,bus,address,ioctl);if(parsed)return result(parsed);
                    const auto service=handle->session->service;uint32_t code=InvalidState;
                    if(applet.live){
                        if(!service->initialized || !service->nvClientRegistered)code=Ipc::NotInitialized;
                        else if(!ioctl.fd || std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)==service->nvDescriptors.end())code=InvalidHandle;
                        else if(ioctl.command==11){
                            code=Ipc::UnknownCommand;
                            const auto slot=size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin());
                            if(ioctl.request==0xC018481B && service->nvDevices[slot]==ServiceState::NvDevice::Channel){
                                const auto as=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),service->nvChannelAddressFds[slot]);
                                if(!service->nvGpfifos[slot] || !service->nvGraphicsContexts[slot].id || !service->nvChannelMapFds[slot] || !service->nvChannelAddressFds[slot] || as==service->nvDescriptors.end())code=InvalidState;
                                else {
                                    std::array<uint8_t,24> parameters{};for(size_t i=0;i<parameters.size();++i)parameters[i]=ram.read8(ioctl.source+i);
                                    std::vector<uint8_t> entries(ioctl.extraSize);for(size_t i=0;i<entries.size();++i)entries[i]=ram.read8(ioctl.extraSource+i);
                                    const auto& registry=service->nvAddressSpaces[size_t(as-service->nvDescriptors.begin())].reservations;
                                    Gpu::SubmissionSnapshot snapshot;
                                    const auto prepared=Gpu::prepareKickoff(parameters,entries,[&](Gpu::VirtualAddress va,std::span<uint8_t> output){
                                        Gpu::AddressSpace::Translation translation;if(!registry || !registry->translate(va,output.size(),translation))return false;
                                        const auto object=std::find_if(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& o){return o.id==translation.object && o.fd==service->nvChannelMapFds[slot];});
                                        return object!=service->nvObjects.end() && readGpuMemory(bus,handle->id,service->nvChannelAddressFds[slot],va,output);
                                    },snapshot);
                                    switch(prepared){
                                    case Gpu::PreflightResult::Success:{
                                        Gpu::ChannelExecution execution;
                                        switch(Gpu::prepareChannelExecution(snapshot,service->nvGpfifos[slot]->channel,execution)){
                                        case Gpu::DecodeResult::Success:{
                                            size_t preparedWrites=0;bool valid=true;
                                            std::vector<uint8_t> copyBytes;
                                            for(const auto& write:execution.writes){
                                                Gpu::AddressSpace::Translation translation;
                                                const auto destination=resolveGpuMemory(bus,handle->id,service->nvChannelAddressFds[slot],write.address,4,Memory::Write);
                                                if(!destination || !registry->translate(write.address,4,translation) ||
                                                   std::none_of(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return object.id==translation.object && object.fd==service->nvChannelMapFds[slot];})){
                                                    code=InvalidAddress;valid=false;break;
                                                }
                                                const auto overlaps=[&](uint64_t start,size_t size){return *destination>=start?*destination-start<size:start-*destination<4;};
                                                if(overlaps(address,Ipc::BufferSize) || overlaps(ioctl.destination,ioctl.size)){code=Ipc::InvalidBuffer;valid=false;break;}
                                                pendingGpuMemory[preparedWrites++]={*destination,write.bytes};
                                            }
                                            if(!valid)break;
                                            if(execution.copy){
                                                const auto& copy=*execution.copy;
                                                const uint64_t sourceSpan=copy.sourceBlockLinear?uint64_t(copy.rows)*copy.bytes:uint64_t(copy.rows-1)*copy.sourcePitch+copy.bytes;
                                                const uint64_t destinationSpan=uint64_t(copy.rows-1)*copy.destinationPitch+copy.bytes;
                                                if(!copy.bytes || !copy.rows || copy.rows>Gpu::CopyRowLimit || uint64_t(copy.bytes)*copy.rows>Gpu::CopyByteLimit || sourceSpan>Gpu::CopyByteLimit || destinationSpan>Gpu::CopyByteLimit){code=Ipc::TransferLimit;break;}
                                                const auto owned=[&](Gpu::VirtualAddress va,size_t span,uint8_t permission)->std::optional<uint64_t>{
                                                    Gpu::AddressSpace::Translation translation;
                                                    const auto cpu=resolveGpuMemory(bus,handle->id,service->nvChannelAddressFds[slot],va,span,permission);
                                                    if(!cpu || !registry->translate(va,span,translation) || std::none_of(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return object.id==translation.object && object.fd==service->nvChannelMapFds[slot];}))return {};
                                                    return cpu;
                                                };
                                                const auto source=owned(copy.source,sourceSpan,Memory::Read);
                                                const auto destination=owned(copy.destination,destinationSpan,Memory::Write);
                                                if(!source || !destination){code=InvalidAddress;break;}
                                                const auto overlaps=[&](uint64_t a,size_t sizeA,uint64_t b,size_t sizeB){return a>=b?a-b<sizeB:b-a<sizeA;};
                                                if(overlaps(*source,sourceSpan,*destination,destinationSpan)){code=Ipc::InvalidArguments;break;}
                                                if(overlaps(*source,sourceSpan,address,Ipc::BufferSize) || overlaps(*destination,destinationSpan,address,Ipc::BufferSize) ||
                                                   overlaps(*source,sourceSpan,ioctl.destination,ioctl.size) || overlaps(*destination,destinationSpan,ioctl.destination,ioctl.size)){code=Ipc::InvalidBuffer;break;}
                                                copyBytes.resize(size_t(copy.bytes)*copy.rows);
                                                for(uint32_t row=0;row<copy.rows;++row)for(uint32_t i=0;i<copy.bytes;++i){
                                                    const auto offset=copy.sourceBlockLinear?Gpu::copyBlockSourceOffset(copy.bytes,i,row):uint64_t(row)*copy.sourcePitch+i;
                                                    copyBytes[size_t(row)*copy.bytes+i]=ram.read8(*source+offset);
                                                }
                                                pendingGpuCopyDestination=*destination;
                                                pendingGpuCopyRows=copy.rows;pendingGpuCopyWidth=copy.bytes;pendingGpuCopyPitch=copy.destinationPitch;
                                            }
                                            if(service->nvGpfifos[slot]->submissions==UINT64_MAX){code=OutOfResource;break;}
                                            pendingGpuExecution=std::make_shared<ServiceState::Gpfifo>(*service->nvGpfifos[slot]);
                                            pendingGpuExecution->channel=execution.state;++pendingGpuExecution->submissions;
                                            pendingGpuExecutionState=service;pendingGpuExecutionSlot=slot;
                                            // Synchronous no-wait/no-increment profile: return the zero input fence unchanged.
                                            // No work or command snapshots survive this request, and SET_REFERENCE is not a syncpoint.
                                            transfer.address=ioctl.destination;transfer.bytes.assign(parameters.begin(),parameters.end());
                                            pendingGpuCopyBytes=std::move(copyBytes);pendingGpuMemoryCount=preparedWrites;code=Success;break;
                                        }
                                        case Gpu::DecodeResult::Unsupported:code=Ipc::UnknownCommand;break;
                                        case Gpu::DecodeResult::Malformed:code=Ipc::InvalidArguments;break;
                                        case Gpu::DecodeResult::TransferLimit:code=Ipc::TransferLimit;break;
                                        case Gpu::DecodeResult::OutOfResource:code=OutOfResource;break;
                                        }break;
                                    }
                                    case Gpu::PreflightResult::InvalidArguments:code=Ipc::InvalidArguments;break;
                                    case Gpu::PreflightResult::InvalidAddress:code=InvalidAddress;break;
                                    case Gpu::PreflightResult::TransferLimit:code=Ipc::TransferLimit;break;
                                    case Gpu::PreflightResult::OutOfResource:code=OutOfResource;break;
                                    }
                                }
                            }
                        }
                        else if(service->nvDevices[size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin())]==ServiceState::NvDevice::Channel){
                            const auto slot=size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin());
                            code=Ipc::UnknownCommand;
                            if(ioctl.request==0x40044801){
                                const auto mapFd=ram.read32(ioctl.source);
                                const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),mapFd);
                                if(!mapFd || found==service->nvDescriptors.end() || service->nvDevices[size_t(found-service->nvDescriptors.begin())]!=ServiceState::NvDevice::Map)code=InvalidHandle;
                                else if(service->nvChannelMapFds[slot])code=Ipc::AlreadyInitialized;
                                else {code=Success;pendingNvChannelState=service;pendingNvChannelSlot=slot;pendingNvChannelMapFd=mapFd;}
                            }else if(ioctl.request==0xC020481A){
                                if(!service->nvChannelMapFds[slot] || !service->nvChannelAddressFds[slot])code=InvalidState;
                                else if(Gpu::findProfile(selectedGpuProfile)->packets!=Gpu::PacketAbi::B06FReference)code=Ipc::UnknownCommand;
                                else if(ram.read32(ioctl.source)!=GpfifoEntries || ram.read32(ioctl.source+4)!=1 || ram.read32(ioctl.source+8) || ram.read32(ioctl.source+20) || ram.read32(ioctl.source+24) || ram.read32(ioctl.source+28))code=Ipc::InvalidArguments;
                                else if(service->nvGpfifos[slot])code=Ipc::AlreadyInitialized;
                                else if(size_t(std::count_if(service->nvGpfifos.begin(),service->nvGpfifos.end(),[](const auto& fifo){return bool(fifo);}))>=GpfifoLimit || nextGpuFence==UINT32_MAX)code=OutOfResource;
                                else {
                                    pendingGpfifo=std::make_shared<ServiceState::Gpfifo>();pendingGpfifo->fence=nextGpuFence;
                                    pendingGpfifo->channel.profile=selectedGpuProfile;
                                    pendingGpfifoState=service;pendingGpfifoSlot=slot;
                                    transfer.address=ioctl.destination;transfer.bytes.resize(32);
                                    for(size_t i=0;i<32;++i)transfer.bytes[i]=ram.read8(ioctl.source+i);
                                    for(size_t i=0;i<4;++i){transfer.bytes[12+i]=uint8_t(nextGpuFence>>(i*8));transfer.bytes[16+i]=0;}
                                    code=Success;
                                }
                            }else if(ioctl.request==0xC0104809){
                                if(!service->nvGpfifos[slot])code=InvalidState;
                                else if(ram.read32(ioctl.source)!=0xB197 || ram.read32(ioctl.source+4))code=Ipc::InvalidArguments;
                                else if(service->nvGraphicsContexts[slot].id)code=Ipc::AlreadyInitialized;
                                else if(nextGpuContext==UINT64_MAX)code=OutOfResource;
                                else {
                                    transfer.address=ioctl.destination;transfer.bytes.resize(16);
                                    for(size_t i=0;i<8;++i)transfer.bytes[i]=ram.read8(ioctl.source+i);
                                    for(size_t i=0;i<8;++i)transfer.bytes[8+i]=uint8_t(nextGpuContext>>(i*8));
                                    pendingGraphicsState=service;pendingGraphicsSlot=slot;code=Success;
                                }
                            }else if(ioctl.request==0xC018480C){
                                if(!service->nvGraphicsContexts[slot].id || !service->nvErrorEvents[slot])code=InvalidState;
                                else if(ram.read64(ioctl.source) || ram.read64(ioctl.source+8) || ram.read32(ioctl.source+16)>1 || ram.read32(ioctl.source+20))code=Ipc::InvalidArguments;
                                else {
                                    transfer.address=ioctl.destination;transfer.bytes.resize(24);
                                    for(size_t i=0;i<24;++i)transfer.bytes[i]=ram.read8(ioctl.source+i);
                                    pendingNotifierState=service;pendingNotifierSlot=slot;pendingNotifierEnabled=bool(ram.read32(ioctl.source+16));code=Success;
                                }
                            }else if(ioctl.request==0x4004480D){
                                const auto priority=ram.read32(ioctl.source);
                                if(!service->nvChannelMapFds[slot])code=InvalidState;
                                else if(priority!=50 && priority!=100 && priority!=150)code=Ipc::InvalidArguments;
                                else {code=Success;pendingPriorityState=service;pendingPrioritySlot=slot;pendingPriority=priority;}
                            }
                        }
                        else if(service->nvDevices[size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin())]==ServiceState::NvDevice::AddressSpace){
                            const auto slot=size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin());
                            if(ioctl.request==0x40284109){
                                bool valid=ram.read32(ioctl.source)==1 && !ram.read32(ioctl.source+4) && ram.read32(ioctl.source+8)==0x10000;
                                for(size_t offset=12;offset<40;offset+=4)valid=valid && !ram.read32(ioctl.source+offset);
                                if(!valid)code=Ipc::InvalidArguments;
                                else if(service->nvAddressSpaces[slot].initialized)code=Ipc::AlreadyInitialized;
                                else {code=Success;pendingNvAddressState=service;pendingNvAddressSlot=slot;pendingNvPageSize=0x10000;}
                            }else if(!service->nvAddressSpaces[slot].initialized)code=Ipc::NotInitialized;
                            else if(ioctl.request==0x40044101){
                                const auto channelFd=ram.read32(ioctl.source);
                                const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),channelFd);
                                if(!channelFd || found==service->nvDescriptors.end() || service->nvDevices[size_t(found-service->nvDescriptors.begin())]!=ServiceState::NvDevice::Channel)code=InvalidHandle;
                                else {
                                    const auto channelSlot=size_t(found-service->nvDescriptors.begin());
                                    if(!service->nvChannelMapFds[channelSlot])code=InvalidState;
                                    else if(service->nvChannelAddressFds[channelSlot])code=Ipc::AlreadyInitialized;
                                    else {code=Success;pendingNvBindingState=service;pendingNvBindingSlot=channelSlot;pendingNvBindingAddressFd=ioctl.fd;}
                                }
                            }
                            else if(ioctl.request==0xC0184102 || ioctl.request==0xC0104103 || ioctl.request==0xC0284106 || ioctl.request==0xC0084105){
                                const auto& current=service->nvAddressSpaces[slot].reservations;
                                auto next=current?std::make_shared<Gpu::AddressSpace>(*current):std::make_shared<Gpu::AddressSpace>();
                                std::vector<uint8_t> data(ioctl.size);for(size_t i=0;i<data.size();++i)data[i]=ram.read8(ioctl.source+i);
                                auto get=[&](size_t at,size_t n=4){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(data[at+i])<<(i*8);return v;};
                                auto translate=[&](Gpu::AddressSpace::Result value){using R=Gpu::AddressSpace::Result;switch(value){case R::Success:return Success;case R::InvalidArguments:return Ipc::InvalidArguments;case R::InvalidAddress:return InvalidAddress;case R::Occupied:return InvalidState;case R::OutOfResource:return OutOfResource;}return InvalidState;};
                                const auto page=Gpu::AddressSpace::PageSize;
                                if(ioctl.request==0xC0184102){
                                    if(!get(0) || get(4)!=page || get(8)>1 || get(12))code=Ipc::InvalidArguments;
                                    else {
                                        Gpu::VirtualAddress output{};const bool fixed=get(8)==1;
                                        code=translate(next->reserve(get(0)*page,fixed?page:get(16,8),fixed?std::optional<Gpu::VirtualAddress>{Gpu::VirtualAddress{get(16,8)}}:std::nullopt,output));
                                        if(!code)for(size_t i=0;i<8;++i)data[16+i]=uint8_t(output.value>>(i*8));
                                    }
                                }else if(ioctl.request==0xC0104103){
                                    if(!get(8) || get(12)!=page)code=Ipc::InvalidArguments;
                                    else code=translate(next->release(Gpu::VirtualAddress{get(0,8)},get(8)*page));
                                }else if(ioctl.request==0xC0084105)code=translate(next->unmap(Gpu::VirtualAddress{get(0,8)}));
                                else {
                                    const auto object=std::find_if(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return object.id && object.id==get(8);});
                                    const auto offset=get(16,8),requested=get(24,8);const bool fixed=(get(0)&1)!=0;
                                    if((get(0)&~uint64_t(5)) || get(4) || get(12)!=page || (!fixed && get(32,8)))code=Ipc::InvalidArguments;
                                    else if(object==service->nvObjects.end())code=InvalidHandle;
                                    else if(!object->backing)code=InvalidState;
                                    else if(object->flags!=1 || offset>object->size || requested>object->size-offset)code=Ipc::InvalidArguments;
                                    else {
                                        const auto bytes=requested?requested:object->size-offset;
                                        if(!bytes || offset%page || bytes%page)code=Ipc::InvalidArguments;
                                        else if(!bus.isRamRange(object->backing+offset,bytes) || !ram.canAccess(object->backing+offset,bytes,Memory::Read|Memory::Write))code=Ipc::InvalidBuffer;
                                        else {
                                            Gpu::VirtualAddress output{};
                                            code=translate(next->map(object->id,object->size,offset,bytes,fixed?std::optional<Gpu::VirtualAddress>{Gpu::VirtualAddress{get(32,8)}}:std::nullopt,output));
                                            if(!code)for(size_t i=0;i<8;++i)data[32+i]=uint8_t(output.value>>(i*8));
                                        }
                                    }
                                }
                                if(!code){pendingNvVaState=service;pendingNvVaSlot=slot;pendingNvVa=std::move(next);transfer.address=ioctl.destination;transfer.bytes=std::move(data);}
                            }else code=Ipc::UnknownCommand;
                        }
                        else if(service->nvDevices[size_t(std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),ioctl.fd)-service->nvDescriptors.begin())]!=ServiceState::NvDevice::Map)code=Ipc::UnknownCommand;
                        else if(ioctl.request==0xC0080101 || ioctl.request==0xC0180105 || ioctl.request==0xC0200104 || ioctl.request==0xC008010E){
                            std::vector<uint8_t> data(ioctl.size);for(size_t i=0;i<data.size();++i)data[i]=ram.read8(ioctl.source+i);
                            auto get=[&](size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(data[at+i])<<(8*i);return v;};
                            auto put=[&](size_t at,uint32_t v){for(size_t i=0;i<4;++i)data[at+i]=uint8_t(v>>(8*i));};
                            if(ioctl.request==0xC0080101){
                                const auto size=get(0);size_t slot=0;uint64_t reserved=0;
                                while(slot<NvMapObjectLimit && service->nvObjects[slot].id)++slot;
                                for(const auto& object:service->nvObjects)if(object.id)reserved+=object.size;
                                if(!size || size%Memory::PAGE_SIZE || size>HeapCapacity)code=Ipc::InvalidArguments;
                                else if(slot==NvMapObjectLimit || reserved+size>HeapCapacity || nextNvMapObject==UINT32_MAX)code=OutOfResource;
                                else {pendingNvMapState=service;pendingNvMapSlot=slot;pendingNvMapObject={nextNvMapObject,ioctl.fd,size};put(4,nextNvMapObject);code=Success;}
                            }else if(ioctl.request==0xC008010E){
                                const auto object=std::find_if(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return object.id==get(4) && object.fd==ioctl.fd;});
                                if(!get(4) || object==service->nvObjects.end())code=InvalidHandle;
                                else if(!object->backing)code=InvalidState;
                                else {put(0,object->id);code=Success;} // Stable local export ID; remote import remains unsupported.
                            }else if(ioctl.request==0xC0200104){
                                size_t slot=0;while(slot<NvMapObjectLimit && (service->nvObjects[slot].id!=get(0) || service->nvObjects[slot].fd!=ioctl.fd))++slot;
                                const auto alignment=get(12);const uint64_t backing=uint64_t(get(24))|(uint64_t(get(28))<<32);
                                if(!get(0) || slot==NvMapObjectLimit)code=InvalidHandle;
                                else if(service->nvObjects[slot].backing)code=InvalidState;
                                else if(get(4) || get(8)>1 || get(16) || get(20) || alignment<Memory::PAGE_SIZE || alignment>0x20000 || (alignment&(alignment-1)) || backing%Memory::PAGE_SIZE)code=Ipc::InvalidArguments;
                                else {
                                    const auto size=service->nvObjects[slot].size;
                                    if(backing<HeapBase || backing>=HeapBase+currentHeapSize || size>HeapBase+currentHeapSize-backing ||
                                       !bus.isRamRange(backing,size) || !ram.canAccess(backing,size,Memory::Read|(get(8)?Memory::Write:0)))code=Ipc::InvalidBuffer;
                                    else if(overlapsNvBacking(backing,size))code=InvalidState;
                                    else {
                                        pendingNvMapState=service;pendingNvMapSlot=slot;pendingNvMapObject=service->nvObjects[slot];
                                        pendingNvMapObject.backing=backing;pendingNvMapObject.flags=get(8);pendingNvMapObject.alignment=alignment;code=Success;
                                    }
                                }
                            }else{
                                size_t slot=0;while(slot<NvMapObjectLimit && (service->nvObjects[slot].id!=get(0) || service->nvObjects[slot].fd!=ioctl.fd))++slot;
                                if(get(4))code=Ipc::InvalidArguments;
                                else if(!get(0) || slot==NvMapObjectLimit)code=InvalidHandle;
                                else if(nvMapRegistered(get(0)))code=InvalidState;
                                else {
                                    const auto size=service->nvObjects[slot].size;pendingNvMapState=service;pendingNvMapSlot=slot;
                                    put(8,0);put(12,0);put(16,size);put(20,0);code=Success;
                                }
                            }
                            if(!code){transfer.address=ioctl.destination;transfer.bytes=std::move(data);}
                        }else code=Ipc::UnknownCommand;
                    }
                    Ipc::managerResponse(response,code);if(!code)response[4]=9;status=0;
                }else{
                auto read=[&](size_t at){uint32_t v=0;for(size_t i=0;i<4;++i)v|=uint32_t(request[at+i])<<(8*i);return v;};
                const auto service=handle->session->service;uint32_t code=InvalidState;bool nvOutput=false;
                if(read(0)==4 && read(4)==0x80000009U && read(8)==4 && read(32)==0x49434653 && !read(36) && read(40)==3 && !read(44)){
                    if(applet.live){
                        auto* memory=findHandle(read(16));
                        if(read(12)!=0xFFFF8001U || !memory || !memory->transferMemory)code=InvalidHandle;
                        else if(read(48)!=memory->transferMemory->size)code=Ipc::InvalidArguments;
                        else if(service->initialized)code=Ipc::AlreadyInitialized;
                        else {code=Success;pendingNvState=service;pendingNvMemory=memory->transferMemory;}
                    }
                }else if(read(0)==4 && read(4)==0x8000000AU && read(8)==1 && read(32)==0x49434653 && !read(36) && read(40)==8 && !read(44)){
                    if(applet.live){
                        const uint64_t resource=read(48)|(uint64_t(read(52))<<32);
                        if(!service->initialized)code=Ipc::NotInitialized;
                        else if(resource!=applet.id)code=Ipc::InvalidArguments;
                        else {code=Success;pendingNvClient=service;}
                    }
                }else{
                    Ipc::Layout layout;const auto parsed=Ipc::parseBuffers(request,&bus,address,layout);if(parsed)return result(parsed);
                    if(applet.live){
                        if(!service->initialized || !service->nvClientRegistered)code=Ipc::NotInitialized;
                        else if(layout.command==0){
                            if(layout.payloadSize || layout.sends!=1 || layout.receives || !layout.sourceSize || layout.sourceSize>64)code=Ipc::InvalidArguments;
                            else {
                                auto matches=[&](std::string_view path){if(layout.sourceSize!=path.size())return false;for(size_t i=0;i<path.size();++i)if(ram.read8(layout.source+i)!=uint8_t(path[i]))return false;return true;};
                                const bool control=matches("/dev/nvhost-ctrl");
                                const bool addressSpace=matches("/dev/nvhost-as-gpu");
                                const bool channel=matches("/dev/nvhost-gpu");
                                if(!control && !addressSpace && !channel && !matches("/dev/nvmap"))code=Ipc::FileNotFound;
                                else {
                                    size_t slot=0;while(slot<NvDescriptorLimit && service->nvDescriptors[slot])++slot;
                                    if(slot==NvDescriptorLimit || nextNvDescriptor==UINT32_MAX)code=OutOfResource;
                                    else {
                                        pendingNvDescriptorState=service;pendingNvDescriptorSlot=slot;pendingNvDescriptor=nextNvDescriptor;pendingNvDevice=channel?ServiceState::NvDevice::Channel:addressSpace?ServiceState::NvDevice::AddressSpace:control?ServiceState::NvDevice::Control:ServiceState::NvDevice::Map;
                                        code=Success;Ipc::valueResponse(response,code,nextNvDescriptor);nvOutput=true; // fd + zero NvError
                                    }
                                }
                            }
                        }else if(layout.command==4){
                            if(layout.payloadSize!=8 || layout.sends || layout.receives)code=Ipc::InvalidArguments;
                            else {
                                const auto fd=read(layout.payload),eventId=read(layout.payload+4);
                                const auto found=std::find(service->nvDescriptors.begin(),service->nvDescriptors.end(),fd);
                                if(!fd || found==service->nvDescriptors.end() || service->nvDevices[size_t(found-service->nvDescriptors.begin())]!=ServiceState::NvDevice::Channel)code=InvalidHandle;
                                else if(eventId!=3)code=Ipc::InvalidArguments;
                                else {
                                    const auto slot=size_t(found-service->nvDescriptors.begin());
                                    if(!service->nvGraphicsContexts[slot].id)code=InvalidState;
                                    else {
                                        for(auto& candidate:handles)if(!candidate.occupied()){pendingSlot=&candidate;break;}
                                        if(!pendingSlot || nextHandle>=0xFFFF8000U)code=OutOfHandles;
                                        else {
                                            pendingEvent=service->nvErrorEvents[slot];
                                            if(!pendingEvent){pendingEvent=std::make_shared<Event>();pendingEvent->serviceManaged=true;pendingEvent->gpuError=true;}
                                            pendingGpuEventState=service;pendingGpuEventSlot=slot;code=Success;
                                            Ipc::copiedHandleResponse(response,code,nextHandle);response[4]=9;nvOutput=true; // copied read handle + zero NvError
                                        }
                                    }
                                }
                            }
                        }else if(layout.command==2){
                            if(layout.payloadSize!=4 || layout.sends || layout.receives)code=Ipc::InvalidArguments;
                            else {
                                const auto fd=read(layout.payload);size_t slot=0;while(slot<NvDescriptorLimit && service->nvDescriptors[slot]!=fd)++slot;
                                if(!fd || slot==NvDescriptorLimit)code=InvalidHandle;
                                else if(std::find(service->nvChannelMapFds.begin(),service->nvChannelMapFds.end(),fd)!=service->nvChannelMapFds.end())code=InvalidState;
                                else if(std::find(service->nvChannelAddressFds.begin(),service->nvChannelAddressFds.end(),fd)!=service->nvChannelAddressFds.end())code=InvalidState;
                                else if(std::any_of(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return object.fd==fd && nvMapRegistered(object.id);}))code=InvalidState;
                                else {
                                    pendingNvDescriptorState=service;pendingNvDescriptorSlot=slot;pendingNvDescriptor=0;
                                    pendingNvRetiredFd=fd;
                                    code=Success;Ipc::managerResponse(response,code);response[4]=9;nvOutput=true; // zero NvError
                                }
                            }
                        }else code=Ipc::UnknownCommand;
                    }
                }
                if(!nvOutput)Ipc::managerResponse(response,code);status=0;
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::Hid || handle->session->service->kind==ServiceState::Kind::HidResource)
            {
                Ipc::HidRequest hid;
                status=Ipc::parseHid(request,handle->session->service->kind==ServiceState::Kind::HidResource,applet.id,hid);
                if(!status) {
                    if(!applet.live) hid.result=InvalidState;
                    else if(hid.createResource) {
                        hid.result=prepareSession(false);
                        if(!hid.result) {
                            pendingHidMemory=hidMemory;
                            if(!pendingHidMemory) {
                                pendingHidMemory=std::make_shared<SharedMemory>();
                                for(size_t i=0;i<HidSharedSize/Memory::PAGE_SIZE;++i) pendingHidMemory->pages.push_back(std::make_shared<uint8_t[]>(Memory::PAGE_SIZE));
                            }
                            pendingSession->service->kind=ServiceState::Kind::HidResource;
                            pendingSession->service->sharedMemory=pendingHidMemory;
                        }
                    } else if(hid.getMemory) {
                        for(auto& candidate:handles) if(!candidate.occupied()) { pendingSlot=&candidate;break; }
                        if(!pendingSlot || nextHandle>=0xFFFF8000U) hid.result=OutOfHandles;
                        else pendingSharedMemory=handle->session->service->sharedMemory;
                    }
                    if(pendingSharedMemory) Ipc::copiedHandleResponse(response,hid.result,nextHandle);
                    else Ipc::managerResponse(response,hid.result,pendingSession ? nextHandle : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::ViRoot || handle->session->service->kind==ServiceState::Kind::ViApplication || handle->session->service->kind==ServiceState::Kind::ViRelay) {
                if(handle->session->service->kind==ServiceState::Kind::ViRelay && request[3]){
                    Ipc::BinderTransaction transaction;
                    const auto parsed=Ipc::parseBinderTransaction(request,bus,address,transaction);
                    if(parsed)return result(parsed);
                    uint32_t code=InvalidState;
                    if(applet.live){
                        if(transaction.object<0x1000 || transaction.object-0x1000>=applet.managedLayers)code=Ipc::InvalidArguments;
                        else {
                            const auto index=size_t(transaction.object-0x1000);auto state=managedLayers[index];
                            if(state.opened && state.strong){
                                bool connected=state.connected;
                                if(transaction.code==14){
                                    Ipc::QueueRegistration registration;Ipc::Transfer reply;
                                    code=Ipc::parseQueueRegistration(bus,transaction,registration,reply);
                                    if(!code && !state.connected)code=Ipc::NotInitialized;
                                    if(!code){
                                        if(registration.present){
                                            if(state.slots[registration.slot])code=Ipc::AlreadyInitialized;
                                            else {
                                                const ServiceState::MapObject* object=nullptr;
                                                for(const auto& endpoint:handles)if(endpoint.session && !endpoint.session->closed && endpoint.session->service->kind==ServiceState::Kind::NvDriver)
                                                    for(const auto& candidate:endpoint.session->service->nvObjects)if(candidate.id==registration.object)object=&candidate;
                                                if(!object)code=InvalidHandle;
                                                else if(!object->backing)code=InvalidState;
                                                else if(object->flags!=1 || (registration.surface->blockLinear?
                                                    (object->size%registration.surface->size || object->size/registration.surface->size<1 || object->size/registration.surface->size>3 || registration.surface->offset>object->size || registration.surface->size>object->size-registration.surface->offset):object->size!=4096U))code=Ipc::InvalidArguments;
                                                else if(std::any_of(state.surfaces.begin(),state.surfaces.end(),[&](const auto& surface){return surface && (surface->width!=registration.surface->width || surface->height!=registration.surface->height || surface->blockLinear!=registration.surface->blockLinear || (surface->blockLinear && surface->object==registration.object && surface->offset==registration.surface->offset));}))code=Ipc::InvalidArguments;
                                                else if(!ram.canAccess(object->backing,object->size,Memory::Read|Memory::Write))code=Ipc::InvalidBuffer;
                                                else {state.slots[registration.slot]=registration.object;state.surfaces[registration.slot]=registration.surface;}
                                            }
                                        }else if(state.acquired&(uint64_t(1)<<registration.slot))code=InvalidState;
                                        else if(!state.slots[registration.slot])code=Ipc::NotInitialized;
                                        else {state.slots[registration.slot]=0;state.surfaces[registration.slot].reset();}
                                        if(!code)transfer=std::move(reply);
                                    }
                                }else if(transaction.code==1 || transaction.code==3 || transaction.code==7 || transaction.code==8){
                                    if(!state.connected)code=Ipc::NotInitialized;
                                    else {
                                        Ipc::Transfer reply;auto nextAcquired=state.acquired;
                                        code=Ipc::buildQueueAccess(bus,transaction,state.slots,state.acquired,reply,nextAcquired,state.surfaces,state.nextSlot);
                                        if(!code && transaction.code==7){
                                            const auto slot=ram.read32(transaction.source+96);const auto id=state.slots[slot];
                                            const auto& surface=*state.surfaces[slot];
                                            const ServiceState::MapObject* object=nullptr;
                                            for(const auto& endpoint:handles)if(endpoint.session && endpoint.session->service->kind==ServiceState::Kind::NvDriver)
                                                for(const auto& candidate:endpoint.session->service->nvObjects)if(candidate.id==id)object=&candidate;
                                            if(!object || !object->backing || object->flags!=1 || surface.offset>object->size || surface.size>object->size-surface.offset || !bus.isRamRange(object->backing+surface.offset,surface.size) || !ram.canAccess(object->backing+surface.offset,surface.size,Memory::Read))code=Ipc::InvalidBuffer;
                                            else {
                                                auto snapshot=std::make_shared<Frame>();snapshot->width=surface.width;snapshot->height=surface.height;snapshot->rgb.resize(size_t(surface.width)*surface.height*3);
                                                for(size_t y=0;y<surface.height;++y)for(size_t x=0;x<surface.width;++x)for(size_t channel=0;channel<3;++channel){
                                                    const auto byte=x*4+channel;
                                                    // 64-byte-wide blocks, sixteen GOBs high, 16Bx2 sector order.
                                                    const auto offset=surface.blockLinear ? (y/128)*(surface.pitch/64)*8192+(byte/64)*8192+((y%128)/8)*512+((byte%64)/32)*256+((y%8)/2)*64+((byte%32)/16)*32+(y%2)*16+byte%16 : y*surface.pitch+byte;
                                                    snapshot->rgb[(y*surface.width+x)*3+channel]=ram.read8(object->backing+surface.offset+offset);
                                                }
                                                pendingFrame=std::move(snapshot);
                                                if(surface.blockLinear)state.nextSlot=(slot+1)%64;
                                            }
                                        }
                                        if(!code){transfer=std::move(reply);state.acquired=nextAcquired;}
                                    }
                                }else if(transaction.code==11 && state.acquired)code=InvalidState;
                                else code=Ipc::buildQueueReply(bus,transaction,state.connected,transfer,connected);
                                if(!code){state.connected=connected;if(!connected){state.slots={};state.surfaces={};state.acquired=0;state.nextSlot=0;}pendingLayer=std::pair{index,state};}
                            }
                        }
                    }
                    Ipc::managerResponse(response,code);status=0;
                }else{
                const bool openLayerPacket=handle->session->service->kind==ServiceState::Kind::ViApplication && (request[7]&0x80);
                Ipc::Layout layout;status=openLayerPacket ? parseViOpenLayer(request,bus,address,layout) : Ipc::parseBuffers(request,nullptr,0,layout);
                if(!status) {
                    const auto service=handle->session->service;
                    uint32_t code=Ipc::UnknownCommand;bool create=false;auto child=ServiceState::Kind::ViApplication;bool value=false;
                    auto read=[&](size_t at,size_t size=8){uint64_t v=0;for(size_t i=0;i<size;++i)v|=uint64_t(request[at+i])<<(8*i);return v;};
                    if(!openLayerPacket && (layout.sends || layout.receives))return result(Ipc::UnsupportedFormat);
                    if(!applet.live)code=InvalidState;
                    else if(service->kind==ServiceState::Kind::ViRoot && layout.command==0) {
                        if(layout.payloadSize!=4 || read(layout.payload,4))code=Ipc::InvalidArguments;
                        else create=true;
                    }else if(service->kind==ServiceState::Kind::ViApplication) {
                        if(openLayerPacket) {
                            if(layout.payloadSize!=80 || layout.destinationSize!=0x100)code=Ipc::InvalidArguments;
                            else {
                                constexpr char displayName[]="Default";bool valid=true;
                                for(size_t i=0;i<64;++i)if(request[layout.payload+i]!=(i<7?uint8_t(displayName[i]):0))valid=false;
                                const auto layer=read(layout.payload+64),resource=read(layout.payload+72);
                                if(!valid)code=Ipc::PortNotFound;
                                else if(!service->displayReferences)code=InvalidState;
                                else if(resource!=applet.id || !layer || layer>applet.managedLayers)code=Ipc::InvalidArguments;
                                else {
                                    const auto index=size_t(layer-1);auto state=managedLayers[index];
                                    if(state.opened)code=Ipc::AlreadyInitialized;
                                    else {
                                        // Explicit local ParcelHeader + three-word producer payload.
                                        // The producer is a real bounded reference object, not a GPU queue.
                                        transfer.address=layout.destination;transfer.bytes.resize(28);
                                        auto put=[&](size_t at,uint32_t v){for(size_t i=0;i<4;++i)transfer.bytes[at+i]=uint8_t(v>>(8*i));};
                                        put(0,12);put(4,16);put(8,0);put(12,28);put(24,uint32_t(0x1000+index));
                                        state.opened=true;state.owner=service;pendingLayer=std::pair{index,state};
                                        code=Success;Ipc::valueResponse(response,code,28);
                                    }
                                }
                            }
                        }else if(layout.command==100) {
                            if(layout.payloadSize)code=Ipc::InvalidArguments;
                            else {create=true;child=ServiceState::Kind::ViRelay;}
                        }else if(layout.command==1010) {
                            if(layout.payloadSize!=64)code=Ipc::InvalidArguments;
                            else {
                                constexpr char displayName[]="Default";bool valid=true;
                                for(size_t i=0;i<64;++i)if(request[layout.payload+i]!=(i<7?uint8_t(displayName[i]):0))valid=false;
                                if(!valid)code=Ipc::PortNotFound;
                                else if(service->displayReferences==64)code=OutOfResource;
                                else {code=Success;value=true;pendingViState=service;pendingViReferences=service->displayReferences+1;}
                            }
                        }else if(layout.command==2021) {
                            if(layout.payloadSize!=8 || !read(layout.payload) || read(layout.payload)>applet.managedLayers)code=Ipc::InvalidArguments;
                            else {
                                const auto index=size_t(read(layout.payload)-1);auto state=managedLayers[index];
                                if(!state.opened || state.owner.lock()!=service || state.connected)code=InvalidState;
                                else {state.opened=false;state.owner.reset();pendingLayer=std::pair{index,state};code=Success;}
                            }
                        }else if(layout.command==1020 || layout.command==1102) {
                            if(layout.payloadSize!=8 || read(layout.payload)!=1)code=Ipc::InvalidArguments;
                            else if(!service->displayReferences)code=InvalidState;
                            else if(layout.command==1020){code=Success;pendingViState=service;pendingViReferences=service->displayReferences-1;}
                        }
                    }
                    if(applet.live && service->kind==ServiceState::Kind::ViRelay && layout.command==1) {
                        if(layout.payloadSize!=12)code=Ipc::InvalidArguments;
                        else {
                            const auto binder=read(layout.payload,4),delta=read(layout.payload+4,4),type=read(layout.payload+8,4);
                            if(binder<0x1000 || binder-0x1000>=applet.managedLayers || type>1 || (delta!=1 && delta!=UINT32_MAX))code=Ipc::InvalidArguments;
                            else {
                                const auto index=size_t(binder-0x1000);auto state=managedLayers[index];auto& refs=type?state.strong:state.weak;
                                if(!state.opened && !state.weak && !state.strong)code=InvalidState;
                                else if(delta==1 && refs==64)code=OutOfResource;
                                else if(delta==UINT32_MAX && !refs)code=InvalidState;
                                else if(delta==UINT32_MAX && type==1 && refs==1 && state.connected)code=InvalidState;
                                else {if(delta==1)++refs;else --refs;pendingLayer=std::pair{index,state};code=Success;}
                            }
                        }
                    }
                    if(applet.live && service->kind==ServiceState::Kind::ViRelay && layout.command==2) {
                        if(layout.payloadSize!=8)code=Ipc::InvalidArguments;
                        else {
                            const auto binder=read(layout.payload,4),selector=read(layout.payload+4,4);
                            if(binder<0x1000 || binder-0x1000>=applet.managedLayers || selector!=0xF)code=Ipc::InvalidArguments;
                            else {
                                const auto index=size_t(binder-0x1000);auto state=managedLayers[index];
                                if(!state.opened || !state.strong)code=InvalidState;
                                else {
                                    for(auto& slot:handles)if(!slot.occupied()){pendingSlot=&slot;break;}
                                    if(!pendingSlot || nextHandle>=0xFFFF8000U)code=OutOfHandles;
                                    else {
                                        if(!state.available){state.available=std::make_shared<Event>();state.available->serviceManaged=true;}
                                        // The event reflects free registered CPU slots; GPU work is unsupported.
                                        pendingEvent=state.available;pendingLayer=std::pair{index,state};code=Success;
                                    }
                                }
                            }
                        }
                    }
                    if(create){code=prepareSession(false);if(!code)pendingSession->service->kind=child;}
                    if(transfer.bytes.empty()) {
                        if(pendingEvent)Ipc::copiedHandleResponse(response,code,nextHandle);
                        else if(value)Ipc::valueResponse(response,code,1);
                        else Ipc::managerResponse(response,code,pendingSession?nextHandle:0);
                    }
                }
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::TimeRoot || handle->session->service->kind==ServiceState::Kind::TimeClock)
            {
                Ipc::TimeRequest time;
                const auto role=handle->session->service->kind==ServiceState::Kind::TimeRoot ? Ipc::TimeRole::Root : static_cast<Ipc::TimeRole>(handle->session->service->timeRole);
                status=Ipc::dispatchTime(request,response,role,virtualNanoseconds,ReferenceUnixEpoch,time);
                if(!status) {
                    if(!applet.live) Ipc::managerResponse(response,InvalidState);
                    else if(time.child!=Ipc::TimeRole::Root) {
                        const auto code=prepareSession(false);
                        if(!code) { pendingSession->service->kind=ServiceState::Kind::TimeClock;pendingSession->service->timeRole=static_cast<uint8_t>(time.child); }
                        Ipc::managerResponse(response,code,pendingSession ? nextHandle : 0);
                    }
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::Apm || handle->session->service->kind==ServiceState::Kind::ApmSession)
            {
                Ipc::ApmRequest apm;
                status=Ipc::parseApm(request,handle->session->service->kind==ServiceState::Kind::ApmSession,apm);
                if(!status) {
                    if(apm.openSession) {
                        apm.result=prepareSession(false);
                        if(pendingSession) pendingSession->service->kind=ServiceState::Kind::ApmSession;
                    }
                    Ipc::managerResponse(response,apm.result,pendingSession ? nextHandle : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::Files || handle->session->service->kind==ServiceState::Kind::File)
            {
                const auto& service=*handle->session->service;
                status=Ipc::dispatchFiles(request,bus,address,service.files.get(),service.file.get(),transfer,fileRequest);
                if(!status) {
                    if(fileRequest.opened) {
                        fileRequest.result=prepareSession(false);
                        if(pendingSession) {
                            pendingSession->service->kind=ServiceState::Kind::File;
                            pendingSession->service->file=fileRequest.opened;
                        }
                    }
                    if(fileRequest.hasValue) Ipc::valueResponse(response,fileRequest.result,fileRequest.value);
                    else Ipc::managerResponse(response,fileRequest.result,pendingSession ? nextHandle : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::Display)
                status=Ipc::dispatchDisplay(request,response,bus,address,pendingFrame);
            else if(handle->session->service->kind==ServiceState::Kind::Input)
                status=Ipc::dispatchInput(request,response,bus,address,inputButtons);
            else status = Ipc::dispatchDemo(request,response,virtualNanoseconds,&bus,address,&transfer);
        }
        catch (const std::bad_alloc&) { return result(OutOfResource); }
        if (status) return result(status); // Do not alter the request on transport error.
        std::array<Memory::WriteRange,2+Gpu::ReferenceWriteLimit+Gpu::CopyRowLimit> writes{};size_t writeCount=0;
        writes[writeCount++]={address,response.data(),response.size()};
        if(!transfer.bytes.empty())writes[writeCount++]={transfer.address,transfer.bytes.data(),transfer.bytes.size()};
        for(size_t i=0;i<pendingGpuMemoryCount;++i)writes[writeCount++]={pendingGpuMemory[i].address,pendingGpuMemory[i].bytes.data(),4};
        if(!pendingGpuCopyBytes.empty())for(uint32_t row=0;row<pendingGpuCopyRows;++row)writes[writeCount++]={pendingGpuCopyDestination+uint64_t(row)*pendingGpuCopyPitch,pendingGpuCopyBytes.data()+size_t(row)*pendingGpuCopyWidth,pendingGpuCopyWidth};
        if (!ram.writeRanges(std::span(writes.data(),writeCount)))
        { ram.clearError(); return result(OutOfResource); }
        // Publish state only after the complete reply reached guest RAM.
        if (pendingSession)
        {
            *pendingSlot = {}; pendingSlot->id = nextHandle++;
            pendingSlot->session = std::move(pendingSession);
        }
        if (manager.initialize) handle->session->service->initialized = true;
        if(pendingFsAttach) pendingFsAttach->initialized=true;
        if(pendingViState)pendingViState->displayReferences=pendingViReferences;
        if(pendingNvState){pendingNvState->nvMemory=std::move(pendingNvMemory);pendingNvState->initialized=true;}
        if(pendingNvClient)pendingNvClient->nvClientRegistered=true;
        if(pendingNvDescriptorState){pendingNvDescriptorState->nvDescriptors[pendingNvDescriptorSlot]=pendingNvDescriptor;pendingNvDescriptorState->nvDevices[pendingNvDescriptorSlot]=pendingNvDevice;pendingNvDescriptorState->nvAddressSpaces[pendingNvDescriptorSlot]={};pendingNvDescriptorState->nvChannelMapFds[pendingNvDescriptorSlot]=0;pendingNvDescriptorState->nvChannelAddressFds[pendingNvDescriptorSlot]=0;pendingNvDescriptorState->nvGpfifos[pendingNvDescriptorSlot].reset();pendingNvDescriptorState->nvGraphicsContexts[pendingNvDescriptorSlot]={};pendingNvDescriptorState->nvErrorEvents[pendingNvDescriptorSlot].reset();pendingNvDescriptorState->nvNotifierEnabled[pendingNvDescriptorSlot]=false;pendingNvDescriptorState->nvPriorities[pendingNvDescriptorSlot]=0;if(pendingNvDescriptor)++nextNvDescriptor;}
        if(pendingNvChannelState)pendingNvChannelState->nvChannelMapFds[pendingNvChannelSlot]=pendingNvChannelMapFd;
        if(pendingNvBindingState)pendingNvBindingState->nvChannelAddressFds[pendingNvBindingSlot]=pendingNvBindingAddressFd;
        if(pendingGpfifoState){pendingGpfifoState->nvGpfifos[pendingGpfifoSlot]=std::move(pendingGpfifo);++nextGpuFence;}
        if(pendingGpuExecutionState)pendingGpuExecutionState->nvGpfifos[pendingGpuExecutionSlot]=std::move(pendingGpuExecution);
        if(pendingGraphicsState)pendingGraphicsState->nvGraphicsContexts[pendingGraphicsSlot]={0xB197,nextGpuContext++};
        if(pendingGpuEventState)pendingGpuEventState->nvErrorEvents[pendingGpuEventSlot]=pendingEvent;
        if(pendingNotifierState)pendingNotifierState->nvNotifierEnabled[pendingNotifierSlot]=pendingNotifierEnabled;
        if(pendingPriorityState)pendingPriorityState->nvPriorities[pendingPrioritySlot]=pendingPriority;
        if(pendingNvAddressState)pendingNvAddressState->nvAddressSpaces[pendingNvAddressSlot]={true,pendingNvPageSize};
        if(pendingNvVaState)pendingNvVaState->nvAddressSpaces[pendingNvVaSlot].reservations=std::move(pendingNvVa);
        if(pendingNvRetiredFd)for(auto& object:pendingNvDescriptorState->nvObjects)if(object.fd==pendingNvRetiredFd)object={};
        if(pendingNvMapState){const bool created=!pendingNvMapState->nvObjects[pendingNvMapSlot].id && pendingNvMapObject.id;pendingNvMapState->nvObjects[pendingNvMapSlot]=pendingNvMapObject;if(created)++nextNvMapObject;}
        if(pendingLayer){
            auto& layer=managedLayers[pendingLayer->first];layer=std::move(pendingLayer->second);
            if(layer.available){
                bool ready=false;if(layer.connected)for(size_t slot=0;slot<layer.slots.size();++slot)if(layer.slots[slot] && !(layer.acquired&(uint64_t(1)<<slot))){ready=true;break;}
                layer.available->signaled=ready;if(ready)resolveWaits();
            }
        }
        if(pendingSharedMemory) { *pendingSlot={};pendingSlot->id=nextHandle++;pendingSlot->sharedMemory=std::move(pendingSharedMemory); }
        if(pendingHidMemory) hidMemory=std::move(pendingHidMemory);
        if(pendingEvent) { *pendingSlot={};pendingSlot->id=nextHandle++;pendingSlot->event=std::move(pendingEvent); }
        if(consumeAppletMessage) { applet.messages.pop_front();applet.event->signaled=!applet.messages.empty(); }
        if(acquireForeground) applet.foregroundRights=true;
        if(createManagedLayer) ++applet.managedLayers;
        if(notifyRunning) applet.runningNotified=true;
        if(pendingSuspendBackground) applet.suspendBackground=*pendingSuspendBackground;
        if(pendingOperationNotifications) applet.operationNotifications=*pendingOperationNotifications;
        if(pendingPerformanceNotifications) applet.performanceNotifications=*pendingPerformanceNotifications;
        if(pendingDomain) handle->session->domain=std::move(pendingDomain);
        if(pendingObject) *pendingObject={objectDomain->nextId++,std::move(pendingObjectService)};
        if(pendingFrame) { lastFrame=std::move(pendingFrame); ++presentedFrames; }
        return result(Success);
    }
    case 0x08: // CreateThread: X1 entry, X2 argument, X3 SP, W4 priority, W5 core.
    {
        const uint64_t entry = cpu.readRegister(1), stack = cpu.readRegister(3);
        if ((entry & 3) || !bus.isRamRange(entry,4) || !ram.canAccess(entry,4,Memory::Execute))
            return result(InvalidAddress);
        // This local profile reserves one separate stack/TLS and one logical core.
        if (stack != WorkerStackTop) return result(InvalidAddress);
        if (cpu.readRegister(4) != 32 || (static_cast<uint32_t>(cpu.readRegister(5)) != 0 &&
            static_cast<uint32_t>(cpu.readRegister(5)) != 0xFFFFFFFEU)) return Action::Unsupported;
        if (workerCreated) return result(OutOfResource);
        Handle* slot = nullptr;
        for (auto& handle : handles) if (!handle.occupied()) { slot = &handle; break; }
        if (!slot || nextHandle >= 0xFFFF8000U) return result(OutOfHandles);
        std::shared_ptr<Event> completion;
        try { completion = std::make_shared<Event>(); }
        catch (const std::bad_alloc&) { return result(OutOfResource); }
        worker.context = {};
        worker.context.pc = entry; worker.context.sp = stack;
        worker.context.registers[0] = cpu.readRegister(2);
        worker.context.tpidrroEl0 = worker.localRegion;
        worker.state = State::Ready;
        workerCreated = true; workerExit = completion;
        *slot = {nextHandle++,false,completion,true};
        cpu.writeRegister(1,slot->id);
        return result(Success);
    }
    case 0x09:
    {
        auto* handle = findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if (!handle || !handle->isThread) return result(InvalidHandle);
        if (handle->isMainThread || started[1] || worker.state == State::Exited) return result(InvalidState);
        started[1] = true;
        return result(Success);
    }
    case 0x0A:
        at(active).state = State::Exited;
        waits[active] = {};
        if (active == 1 && workerExit) workerExit->signaled = true;
        if (active == 0 && mainExit) mainExit->signaled = true;
        return Action::ThreadExit;
    case 0x25: // GetThreadId: input handle in W1, output ID in X1.
    {
        auto* handle=findHandle(static_cast<uint32_t>(cpu.readRegister(1)));
        if(!handle || !handle->isThread) return result(InvalidHandle);
        cpu.writeRegister(1,handle->isMainThread ? thread.id : worker.id);
        return result(Success);
    }
    case 0x0B:
    {
        const auto duration = cpu.readRegister(0);
        if (duration == 0) return Action::Yield;
        // Negative yield variants need scheduling policies absent from this profile.
        if (duration > INT64_MAX || duration > UINT64_MAX-virtualNanoseconds) return Action::Unsupported;
        auto& wait = waits[active];
        wait = {}; wait.pending = true; wait.timed = true; wait.sleep = true;
        wait.deadline = virtualNanoseconds + duration;
        at(active).state = State::Waiting;
        return Action::Wait;
    }
    case 0x1E: // Local virtual tick counter at the Horizon reference frequency (19.2 MHz).
        cpu.writeRegister(0,systemTick());
        return Action::Continue;
    case 0x45: // CreateEvent: typed endpoints sharing one manual-reset event.
    {
        Handle* first = nullptr; Handle* second = nullptr;
        for (auto& handle : handles) if (!handle.occupied())
        { if (!first) first = &handle; else { second = &handle; break; } }
        // IDs are never recycled in a process, so a closed handle stays invalid.
        if (!second || nextHandle >= 0xFFFF8000U - 1) return result(OutOfHandles);
        std::shared_ptr<Event> event;
        try { event = std::make_shared<Event>(); }
        catch (const std::bad_alloc&) { return result(OutOfResource); }
        *first = {nextHandle++,true,event}; *second = {nextHandle++,false,event};
        cpu.writeRegister(1,first->id); cpu.writeRegister(2,second->id);
        return result(Success);
    }
    case 0x11:
    {
        const auto code = signalEvent(static_cast<uint32_t>(cpu.readRegister(0)));
        result(code);
        // Resolve waiters before the signaling thread can clear the event again.
        return code == Success ? Action::Yield : Action::Continue;
    }
    case 0x12: // ClearEvent accepts either event endpoint.
    case 0x17: // ResetSignal requires a readable, signaled event.
    {
        auto* handle = findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if (!handle || !handle->event || handle->isThread || (id == 0x17 && handle->writable)) return result(InvalidHandle);
        if(handle->event->serviceManaged) return result(InvalidState);
        if (id == 0x17 && !handle->event->signaled) return result(InvalidState);
        handle->event->signaled = false;
        return result(Success);
    }
    case 0x13: // Bounded HID shared-memory mapping, read-only in the guest.
    case 0x14:
    {
        if(!homebrewProfile) return Action::Unsupported;
        auto* handle=findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if(!handle || !handle->sharedMemory) return result(InvalidHandle);
        if(!applet.live) return result(InvalidState);
        const auto address=cpu.readRegister(1),size=cpu.readRegister(2);
        if(address%Memory::PAGE_SIZE) return result(InvalidAddress);
        if(size!=HidSharedSize) return result(InvalidSize);
        auto& shared=*handle->sharedMemory;
        if(id==0x14) {
            if(!shared.mapping || *shared.mapping!=address) return result(InvalidState);
            if(!ram.discardPages(address,HidSharedSize)) { ram.clearError();return result(InvalidAddress); }
            for(size_t page=size_t(address/Memory::PAGE_SIZE);page<size_t((address+size)/Memory::PAGE_SIZE);++page) { types[page]=0;memoryAttributes[page]=0; }
            shared.mapping.reset();return result(Success);
        }
        if(cpu.readRegister(3)!=Memory::Read) return result(InvalidState);
        if(shared.mapping) return result(InvalidState); // One mapping per object in this reference profile.
        if(!bus.isRamRange(address,size_t(size))) return result(InvalidAddress);
        const auto overlaps=[&](uint64_t begin,uint64_t end) { return address<end && address+size>begin; };
        if(overlaps(HeapBase,AliasBase+AliasSize) || overlaps(WorkerStackBase-Memory::PAGE_SIZE,WorkerTlsPage+Memory::PAGE_SIZE) ||
           overlaps(HomebrewEnvironment,HomebrewReturn+Memory::PAGE_SIZE) || overlaps(thread.localRegion-Memory::PAGE_SIZE,ram.size())) return result(InvalidAddress);
        for(size_t page=size_t(address/Memory::PAGE_SIZE);page<size_t((address+size)/Memory::PAGE_SIZE);++page)
            if(types[page]!=0 || memoryAttributes[page]!=0) return result(InvalidAddress);
        if(!ram.mapReadOnlySharedPages(address,shared.pages)) { ram.clearError();return result(InvalidAddress); }
        for(size_t page=size_t(address/Memory::PAGE_SIZE);page<size_t((address+size)/Memory::PAGE_SIZE);++page) types[page]=6;
        shared.mapping=address;return result(Success);
    }
    case 0x15: // Scoped Perm_None transfer memory over writable heap pages.
    {
        if(!homebrewProfile)return Action::Unsupported;
        const auto address=cpu.readRegister(1),size=cpu.readRegister(2),permission=cpu.readRegister(3);
        auto fail=[&](uint32_t code){cpu.writeRegister(1,0);return result(code);};
        if(!applet.live)return fail(InvalidState);
        if(address%Memory::PAGE_SIZE)return fail(InvalidAddress);
        if(!size || size%Memory::PAGE_SIZE)return fail(InvalidSize);
        if(permission!=Memory::None)return fail(InvalidState);
        if(address<HeapBase || address>=HeapBase+currentHeapSize || size>HeapBase+currentHeapSize-address || !bus.isRamRange(address,size_t(size)))return fail(InvalidAddress);
        if(overlapsNvBacking(address,size))return fail(InvalidState);
        const auto first=size_t(address/Memory::PAGE_SIZE),end=first+size_t(size/Memory::PAGE_SIZE);
        for(size_t page=first;page<end;++page)if(types[page]!=5 || memoryAttributes[page] || ram.permissionsAt(page*Memory::PAGE_SIZE)!=(Memory::Read|Memory::Write))return fail(InvalidState);
        Handle* slot=nullptr;for(auto& candidate:handles)if(!candidate.occupied()){slot=&candidate;break;}
        if(!slot || nextHandle>=0xFFFF8000U)return fail(OutOfHandles);
        std::shared_ptr<TransferMemory>* registry=nullptr;for(auto& candidate:transfers)if(!candidate){registry=&candidate;break;}
        if(!registry)return fail(OutOfResource);
        std::shared_ptr<TransferMemory> memory;
        try{memory=std::make_shared<TransferMemory>(TransferMemory{address,size});}catch(const std::bad_alloc&){return fail(OutOfResource);}
        if(!ram.setPermissions(address,size_t(size),Memory::None))return fail(OutOfResource);
        for(size_t page=first;page<end;++page)memoryAttributes[page]=1; // Local borrowed-page state.
        transferMemoryOwner=&ram;*registry=memory;*slot={};slot->id=nextHandle++;slot->transferMemory=std::move(memory);
        cpu.writeRegister(1,slot->id);return result(Success);
    }
    case 0x16:
    {
        auto* handle = findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if (!handle) return result(InvalidHandle);
        if(handle->session && handle->session->service->kind==ServiceState::Kind::NvDriver){
            const auto service=handle->session->service;
            const bool registered=std::any_of(service->nvObjects.begin(),service->nvObjects.end(),[&](const auto& object){return nvMapRegistered(object.id);});
            const bool sibling=std::any_of(handles.begin(),handles.end(),[&](const auto& other){return other.id!=handle->id && other.session && other.session->service==service;});
            if(registered && !sibling)return result(InvalidState);
        }
        *handle = {};
        if(!collectTransferMemory(ram))return result(OutOfResource);
        return result(Success);
    }
    case 0x18: // WaitSynchronization: poll, finite timeout or explicit -1 infinite wait.
    {
        const uint32_t count = static_cast<uint32_t>(cpu.readRegister(2));
        if (count > waits[active].events.size()) return result(TooManyHandles);
        const uint64_t address = cpu.readRegister(1), timeout = cpu.readRegister(3);
        if (count && (!bus.isRamRange(address,count*4) || !ram.canAccess(address,count*4,Memory::Read)))
            return result(InvalidPointer);
        std::array<std::shared_ptr<Event>,64> events{};
        for (size_t i = 0; i < count; ++i)
        {
            auto* handle = findHandle(ram.read32(address+i*4));
            if (!handle || !handle->event || handle->writable) return result(InvalidHandle);
            events[i] = handle->event;
        }
        for (size_t i = 0; i < count; ++i) if (events[i]->signaled)
        { cpu.writeRegister(1,i); return result(Success); }
        if (timeout == 0) return result(TimedOut);
        if (timeout != UINT64_MAX && (timeout > INT64_MAX || timeout > UINT64_MAX-virtualNanoseconds))
            return Action::Unsupported;
        waits[active] = {std::move(events),count,true};
        waits[active].timed = timeout != UINT64_MAX;
        if (waits[active].timed) waits[active].deadline = virtualNanoseconds + timeout;
        at(active).state = State::Waiting;
        return Action::Wait;
    }
    case 0x01: // SetHeapSize: size in X1; Result in W0, address in X1.
    {
        const uint64_t size = cpu.readRegister(1);
        if (size > 0x100000000ULL || size % HeapAlignment != 0) return result(InvalidSize);
        if (size > HeapCapacity) return result(OutOfMemory);
        if (size > currentHeapSize)
        {
            if (!ram.setPermissions(HeapBase+currentHeapSize,static_cast<size_t>(size-currentHeapSize),Memory::Read | Memory::Write))
                return result(OutOfResource);
        }
        else if (size < currentHeapSize)
        {
            if(overlapsNvBacking(HeapBase+size,currentHeapSize-size))return result(InvalidState);
            for(const auto& memory:transfers)if(memory && memory->address+memory->size>HeapBase+size)return result(InvalidState);
            if (!ram.discardPages(HeapBase+size,static_cast<size_t>(currentHeapSize-size))) return result(OutOfResource);
        }
        for (uint64_t page = HeapBase / Memory::PAGE_SIZE; page < (HeapBase+HeapCapacity) / Memory::PAGE_SIZE; ++page)
            types[static_cast<size_t>(page)] = page * Memory::PAGE_SIZE < HeapBase + size ? 5 : 0;
        currentHeapSize = size;
        cpu.writeRegister(1,HeapBase);
        return result(Success);
    }
    case 0x06: // QueryMemory: output struct at X0, queried address X2; PageInfo returned in W1.
    {
        const uint64_t output = cpu.readRegister(0), address = cpu.readRegister(2);
        if (!ram.isValidAddress(address)) return result(InvalidAddress); // bounded local address space
        if (!bus.isRamRange(output,40) || !ram.canAccess(output,40,Memory::Write)) return result(InvalidPointer);
        const size_t page = static_cast<size_t>(address / Memory::PAGE_SIZE);
        const uint32_t type = types[page];
        const uint8_t permission = ram.permissionsAt(address);
        const auto attributes=memoryAttributes[page];
        auto matches = [&](size_t index) {
            return types[index] == type && memoryAttributes[index]==attributes && ram.permissionsAt(index * Memory::PAGE_SIZE) == permission;
        };
        size_t begin = page, end = page + 1;
        while (begin > 0 && matches(begin-1)) --begin;
        while (end < types.size() && matches(end)) ++end;
        std::array<uint8_t,40> bytes{};
        auto encode = [&](size_t at,uint64_t value,size_t count) {
            for (size_t i=0;i<count;++i) bytes[at+i] = static_cast<uint8_t>(value >> (8*i));
        };
        encode(0,begin * Memory::PAGE_SIZE,8); encode(8,(end-begin) * Memory::PAGE_SIZE,8);
        encode(16,type,4); encode(20,attributes,4); encode(24,permission,4);
        if (!ram.writeBytes(output,bytes.data(),bytes.size())) { ram.clearError(); return result(OutOfResource); }
        cpu.writeRegister(1,0);
        return result(Success);
    }
    case 0x07: // ExitProcess has no input status code.
        return Action::Exit;
    default:
        return Action::Unsupported;
    }
}
