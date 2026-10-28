#pragma once
#include "gpu_classes.hpp"
#include "gpu_reference.hpp"
#include "gpu_maxwell_copy.hpp"
#include <optional>
namespace Gpu {
struct ChannelState {
    ProfileId profile=ProfileId::MaxwellReference;
    ReferenceState reference;
    std::array<uint32_t,8> bindings{}; // Zero means no engine object is bound.
    std::array<MaxwellCopyState,8> copy{};
    bool operator==(const ChannelState&) const=default;
};
// bytes is the width of one row; the single-line form ignores pitch registers.
struct CopyRequest { VirtualAddress source,destination;uint32_t bytes=0,rows=1,sourcePitch=0,destinationPitch=0;bool sourceBlockLinear=false; };
struct ChannelExecution { ChannelState state;std::vector<ReferenceMemoryWrite> writes;std::optional<CopyRequest> copy; };
// Decode once, dispatch by exact class, and publish no effects until the whole stream succeeds.
DecodeResult prepareChannelExecution(const SubmissionSnapshot& input,const ChannelState& state,ChannelExecution& output);
}
