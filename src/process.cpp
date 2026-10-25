#include "process.hpp"
#include "memory.hpp"
#include "ipc.hpp"
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
}

bool Process::initialize(Memory& ram, const ProgramLoader::Image& image, bool reference, std::string& error, bool homebrew)
{
    error.clear();
    if(homebrew && (!reference || image.format!=ProgramLoader::Format::Nro))
    { error="Profil homebrew : NRO et HLE de reference requis";return false; }
    ProgramLoader::RuntimeLayout layout;
    if(homebrew && !ProgramLoader::validateNroRuntime(ram,image,error,&layout)) return false;
    if(image.requiresNroRuntime && !homebrew)
    { error="NRO MOD0 : runtime homebrew (environnement et relocations libnx) non pris en charge";return false; }
    if (reference && ((image.format != ProgramLoader::Format::Elf64 && image.format != ProgramLoader::Format::Nro) || ram.size() < HeapBase + HeapCapacity + StackSize + 2 * Memory::PAGE_SIZE))
    { error = "HLE de reference : ELF/NRO statique et espace pour tas/pile/TLR requis"; return false; }
    const uint64_t tlsPage = ram.size() >= StackSize + 2 * Memory::PAGE_SIZE ? ram.size() - StackSize - 2 * Memory::PAGE_SIZE : 0;
    if (reference)
        for (const auto& region : image.regions)
            if ((region.begin < HeapBase + HeapCapacity && region.end > HeapBase) ||
                (region.begin < tlsPage + Memory::PAGE_SIZE && region.end > tlsPage) ||
                (region.begin < WorkerTlsPage + Memory::PAGE_SIZE && region.end > WorkerStackBase-Memory::PAGE_SIZE) ||
                (homebrew && region.begin<HomebrewReturn+Memory::PAGE_SIZE && region.end>HomebrewEnvironment) ||
                (homebrew && region.begin<AliasBase+AliasSize && region.end>AliasBase))
            { error = "HLE de reference : segment en conflit avec le tas, une TLR ou la pile du second thread"; return false; }
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
    if(homebrew) {
        applet.event=std::make_shared<Event>();applet.event->serviceManaged=true;
        mainExit=std::make_shared<Event>();
        auto& handle=handles[0];handle.id=nextHandle++;handle.event=mainExit;handle.isThread=true;handle.isMainThread=true;
        mainThreadHandle=handle.id;
        if(!ram.setPermissions(HeapBase,HeapAlignment,Memory::Read|Memory::Write) ||
           !ram.setPermissions(HomebrewEnvironment,Memory::PAGE_SIZE,Memory::Read|Memory::Write) ||
           !ram.setPermissions(HomebrewReturn,Memory::PAGE_SIZE,Memory::Read|Memory::Write))
        { error="Profil homebrew : mapping de l'environnement impossible";return false; }
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
        { error="Profil homebrew : preparation de l'environnement impossible";return false; }
        types[size_t(HomebrewEnvironment/Memory::PAGE_SIZE)]=3;types[size_t(HomebrewReturn/Memory::PAGE_SIZE)]=3;
        applet.id=allocateAppletResource();
        if(!applet.id) { error="Identifiants de ressource applet epuises";return false; }
        applet.live=true;
    }
    return true;
}

bool Process::setAppletFocus(bool focused)
{
    if(!applet.live) return false;
    if(applet.focused==focused) return true;
    if(applet.messages.size()==AppletMessageLimit) return false;
    try { applet.messages.push_back(15); }
    catch(const std::bad_alloc&) { return false; }
    applet.focused=focused;applet.event->signaled=true;resolveWaits();return true;
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

void Process::checkpoint(const CPU& cpu, State next)
{
    processState = next;
    at(active).context = cpu.captureContext();
    if (next == State::Exited || next == State::Faulted)
    {
        applet.live=false;applet.messages.clear();if(applet.event) applet.event->signaled=false;
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
        std::shared_ptr<Event> pendingEvent;
        bool consumeAppletMessage=false;
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
                        handle->session->closed = true;
                        handle->session->domain.reset();
                        return result(Success); // Close has no CMIF reply; CloseHandle releases the slot.
                    }
                    if (control.clone) control.result = prepareSession(true);
                    if(control.convert) {
                        if(!homebrewProfile || handle->session->service->kind!=ServiceState::Kind::AppletRoot)
                            control.result=Ipc::UnknownCommand;
                        else if(handle->session->domain) control.result=InvalidState;
                        else {
                            pendingDomain=std::make_shared<Domain>();
                            pendingDomain->objects[0]={1,handle->session->service};
                        }
                    }
                    Ipc::managerResponse(response,control.result,pendingSession ? nextHandle : 0);
                    if(pendingDomain) { response[4]=9;response[32]=1; }
                    // No static pointer buffer is implemented. Return a u16 zero, not the A/B transfer limit.
                    if (control.queryPointerSize && !control.result) response[4] = 9;
                }
            }
            else if(handle->session->domain)
            {
                Ipc::DomainRequest domainRequest;
                status=Ipc::parseDomain(request,domainRequest);
                if(!status) {
                    objectDomain=handle->session->domain;
                    Domain::Object* object=nullptr;
                    for(auto& candidate:objectDomain->objects)
                        if(candidate.id==domainRequest.object && candidate.service) { object=&candidate;break; }
                    if(domainRequest.close) {
                        if(!object) return result(Ipc::InvalidObject);
                        *object={};return result(Success); // Object close keeps the session and has no CMIF response body.
                    }
                    uint32_t code=object ? Ipc::UnknownCommand : Ipc::InvalidObject;
                    uint64_t scalar=0;size_t scalarSize=0;
                    bool open=false;
                    auto kind=ServiceState::Kind::Demo;
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
                    if(object && object->service->kind==ServiceState::Kind::WindowController && domainRequest.command==1) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else { code=Success;scalar=applet.id;scalarSize=8; }
                    }
                    if(object && object->service->kind==ServiceState::Kind::CommonState &&
                       (domainRequest.command==0 || domainRequest.command==1 || domainRequest.command==9)) {
                        if(!applet.live) code=InvalidState;
                        else if(domainRequest.sendPid || domainRequest.payloadSize) code=Ipc::InvalidArguments;
                        else if(domainRequest.command==0) {
                            for(auto& candidate:handles) if(!candidate.occupied()) { pendingSlot=&candidate;break; }
                            if(!pendingSlot || nextHandle>=0xFFFF8000U) code=OutOfHandles;
                            else { code=Success;pendingEvent=applet.event; }
                        } else if(domainRequest.command==9) { code=Success;scalar=applet.focused?1:2;scalarSize=1; }
                        else if(applet.messages.empty()) code=0x680; // AM NoMessages, consumed by the original applet loop.
                        else { code=Success;scalar=applet.messages.front();scalarSize=4;consumeAppletMessage=true; }
                    }
                    if(open) {
                        for(auto& candidate:objectDomain->objects) if(!candidate.service) { pendingObject=&candidate;break; }
                        if(!pendingObject || objectDomain->nextId==UINT32_MAX) { pendingObject=nullptr;code=Ipc::OutOfObjects; }
                        else {
                            pendingObjectService=std::make_shared<ServiceState>();pendingObjectService->kind=kind;code=Success;
                        }
                    }
                    if(scalarSize) Ipc::domainValueResponse(response,code,scalar,scalarSize);
                    else if(pendingEvent) Ipc::domainEventResponse(response,code,nextHandle);
                    else Ipc::domainResponse(response,code,pendingObject ? objectDomain->nextId : 0);
                }
            }
            else if(handle->session->service->kind==ServiceState::Kind::AppletRoot) status=Ipc::UnsupportedFormat;
            else if (handle->session->service->kind == ServiceState::Kind::Manager)
            {
                status = Ipc::parseManager(request,handle->session->service->initialized,manager);
                if (!status)
                {
                    if (manager.openDemo || manager.openFiles || manager.openDisplay || manager.openInput || manager.openApm || manager.openApplet)
                    {
                        if(manager.openFiles && !readOnlyFiles) manager.result=Ipc::FilesDisabled;
                        else if((manager.openApm || manager.openApplet) && !homebrewProfile) manager.result=Ipc::PortNotFound;
                        else {
                            manager.result = prepareSession(false);
                            if(pendingSession && manager.openFiles) {
                                pendingSession->service->kind=ServiceState::Kind::Files;
                                pendingSession->service->files=readOnlyFiles;
                            }
                            if(pendingSession && manager.openDisplay) pendingSession->service->kind=ServiceState::Kind::Display;
                            if(pendingSession && manager.openInput) pendingSession->service->kind=ServiceState::Kind::Input;
                            if(pendingSession && manager.openApm) pendingSession->service->kind=ServiceState::Kind::Apm;
                            if(pendingSession && manager.openApplet) pendingSession->service->kind=ServiceState::Kind::AppletRoot;
                        }
                    }
                    Ipc::managerResponse(response,manager.result,pendingSession ? nextHandle : 0);
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
        const std::array<Memory::WriteRange,2> writes{{
            {address,response.data(),response.size()},
            {transfer.address,transfer.bytes.data(),transfer.bytes.size()}}};
        if (!ram.writeRanges(std::span(writes.data(),transfer.bytes.empty() ? 1 : 2)))
        { ram.clearError(); return result(OutOfResource); }
        // Publish state only after the complete reply reached guest RAM.
        if (pendingSession)
        {
            *pendingSlot = {}; pendingSlot->id = nextHandle++;
            pendingSlot->session = std::move(pendingSession);
        }
        if (manager.initialize) handle->session->service->initialized = true;
        if(pendingEvent) { *pendingSlot={};pendingSlot->id=nextHandle++;pendingSlot->event=std::move(pendingEvent); }
        if(consumeAppletMessage) { applet.messages.pop_front();applet.event->signaled=!applet.messages.empty(); }
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
    case 0x16:
    {
        auto* handle = findHandle(static_cast<uint32_t>(cpu.readRegister(0)));
        if (!handle) return result(InvalidHandle);
        *handle = {};
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
