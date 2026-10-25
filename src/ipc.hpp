#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "read_only_files.hpp"
#include "frame.hpp"
class Bus;

// Bounded HIPC/CMIF reference subset. Service and error codes are local.
namespace Ipc {
constexpr size_t BufferSize = 0x100;
using Buffer = std::array<uint8_t,BufferSize>;
constexpr uint32_t InvalidMessage = 0x3FF, UnsupportedFormat = 0x5FF,
    UnknownCommand = 0x7FF, PortNotFound = 0x9FF, InvalidArguments = 0xBFF;
constexpr uint32_t InvalidBuffer = 0xDFF, TransferLimit = 0xFFF;
constexpr uint32_t NotInitialized = 0x11FF, AlreadyInitialized = 0x13FF;
constexpr uint32_t SessionClosed = 0x15FF;
constexpr uint32_t FilesDisabled = 0x17FF, InvalidPath = 0x19FF, FileNotFound = 0x1BFF;
constexpr uint32_t InvalidObject = 0x1DFF, OutOfObjects = 0x1FFF;
constexpr size_t MaxTransfer = 64*1024;
struct Transfer { uint64_t address = 0; std::vector<uint8_t> bytes; };
struct ManagerRequest { uint32_t result = 0; bool initialize = false, openDemo = false, openFiles = false, openDisplay = false, openInput = false, openApm = false, openApplet = false; };
struct DomainRequest {
    bool close=false, sendPid=false;
    uint32_t object=0,command=0,copyHandle=0;
    size_t payload=0,payloadSize=0;
};
uint32_t parseDomain(const Buffer& request,DomainRequest& plan);
void domainResponse(Buffer& response,uint32_t result,uint32_t object=0);
void domainValueResponse(Buffer& response,uint32_t result,uint64_t value,size_t size);
void domainEventResponse(Buffer& response,uint32_t result,uint32_t handle);
struct ApmRequest { uint32_t result=0;bool openSession=false; };
uint32_t parseApm(const Buffer& request,bool session,ApmRequest& plan);
uint32_t dispatchInput(const Buffer& request,Buffer& response,Bus& bus,uint64_t ipcAddress,uint32_t buttons);
uint32_t dispatchDisplay(const Buffer& request,Buffer& response,Bus& bus,uint64_t ipcAddress,
    std::shared_ptr<const Frame>& frame);
struct Layout {
    uint32_t command = 0; size_t payload = 0, payloadSize = 0;
    size_t sends = 0, receives = 0;
    uint64_t source = 0, sourceSize = 0, destination = 0, destinationSize = 0;
};
uint32_t parseBuffers(const Buffer& request,Bus* bus,uint64_t ipcAddress,Layout& layout);
struct FileRequest {
    uint32_t result = 0; std::shared_ptr<const ReadOnlyFiles::Bytes> opened;
    bool hasValue = false; uint64_t value = 0;
};
uint32_t dispatchFiles(const Buffer& request,Bus& bus,uint64_t ipcAddress,
    const ReadOnlyFiles* files,const ReadOnlyFiles::Bytes* file,Transfer& transfer,FileRequest& plan);
void valueResponse(Buffer& response,uint32_t result,uint64_t value);
struct ControlRequest { uint32_t result = 0; bool close = false, clone = false, queryPointerSize = false, convert = false; };
uint32_t parseControl(const Buffer& request, ControlRequest& plan);
uint32_t parseManager(const Buffer& request, bool initialized, ManagerRequest& plan);
void managerResponse(Buffer& response, uint32_t result, uint32_t movedHandle = 0);
uint32_t dispatchDemo(const Buffer& request, Buffer& response, uint64_t virtualNanoseconds,
    Bus* bus = nullptr, uint64_t ipcAddress = 0, Transfer* transfer = nullptr);
}
