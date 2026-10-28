#pragma once
#include "gpu_va.hpp"
#include <functional>
#include <span>
#include <vector>
namespace Gpu {
struct CommandBufferSnapshot { VirtualAddress address;std::vector<uint8_t> bytes; };
struct SubmissionSnapshot { std::vector<uint64_t> descriptors;std::vector<CommandBufferSnapshot> buffers;uint32_t flags=0; };
enum class PreflightResult { Success,InvalidArguments,InvalidAddress,TransferLimit,OutOfResource };
using CommandReader=std::function<bool(VirtualAddress,std::span<uint8_t>)>;
// Owned, opaque byte copies only. Success does not admit a job or execute methods.
PreflightResult prepareKickoff(std::span<const uint8_t> parameters,std::span<const uint8_t> entries,const CommandReader& reader,SubmissionSnapshot& output);
}
