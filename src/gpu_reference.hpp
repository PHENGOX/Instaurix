#pragma once
#include "gpu_packets.hpp"
#include <array>
namespace Gpu {
// Host-side reference interpreter state; deliberately not a channel completion fence.
struct ReferenceState { uint32_t value=0;bool written=false;bool operator==(const ReferenceState&) const=default; };
inline constexpr size_t ReferenceWriteLimit=256;
struct ReferenceMemoryWrite { VirtualAddress address;std::array<uint8_t,4> bytes{}; };
struct ReferenceExecution { ReferenceState state;std::vector<ReferenceMemoryWrite> writes; };
class ReferenceInterpreter {
public:
    explicit ReferenceInterpreter(ReferenceState initial):reference(initial){}
    DecodeResult apply(const MethodWrite& method,std::vector<ReferenceMemoryWrite>& writes);
    DecodeResult finish() const { return dirty?DecodeResult::Unsupported:DecodeResult::Success; }
    ReferenceState state() const { return reference; }
private:
    ReferenceState reference;
    uint32_t upper=0,lower=0,payload=0;unsigned configured=0;bool dirty=false;
};
// Prepare bounded B06F reference/semaphore effects without touching RAM or caller state.
DecodeResult prepareReferenceExecution(const SubmissionSnapshot& input,const ReferenceState& state,ReferenceExecution& output);
// Execute only SET_REFERENCE on subchannel zero and DMA_NOP, committing as one unit.
// This reference-only helper rejects streams containing memory writes.
DecodeResult executeReferenceStream(const SubmissionSnapshot& input,ReferenceState& state);
}
