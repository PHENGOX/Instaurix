#include "gpu_channel.hpp"
#include <new>
#include <utility>
namespace Gpu {
DecodeResult prepareChannelExecution(const SubmissionSnapshot& input,const ChannelState& state,ChannelExecution& output){
    const auto profile=findProfile(state.profile);
    if(!profile || profile->packets!=PacketAbi::B06FReference)return DecodeResult::Unsupported;
    // Reject inconsistent host-supplied state before interpreting any commands.
    for(const auto id:state.bindings)if(id){const auto info=findClass(id);if(!info || !profileAllows(state.profile,id) || info->engine==EngineKind::Channel || !info->handler)return DecodeResult::Unsupported;}
    DecodedPushbuffer decoded;const auto result=decodePushbuffer(input,decoded);if(result!=DecodeResult::Success)return result;
    try {
        ChannelExecution staged;staged.state=state;ReferenceInterpreter host(state.reference);
        for(const auto& method:decoded.methods){
            if(method.method==0){
                // Only the plain class-ID form is admitted; alternate ENGINE encodings are unsupported.
                if(method.value&0xFFFF0000U)return DecodeResult::Unsupported;
                const auto info=findClass(method.value);
                if(!info || !profileAllows(state.profile,info->id) || info->engine==EngineKind::Channel || !info->handler)return DecodeResult::Unsupported;
                staged.state.bindings[method.subchannel]=info->id;
                staged.state.copy[method.subchannel]={}; // Rebinding starts a fresh local register bank.
            }else if(method.method<0x100){
                // Preserve the original subchannel-zero profile, with deko3d's FIFO subchannel six.
                if(method.subchannel!=0 && method.subchannel!=6)return DecodeResult::Unsupported;
                auto local=method;local.subchannel=0;
                const auto status=host.apply(local,staged.writes);if(status!=DecodeResult::Success)return status;
            }else{
                const auto info=findClass(staged.state.bindings[method.subchannel]);if(!info)return DecodeResult::Unsupported;
                const auto status=info->handler(method,staged);if(status!=DecodeResult::Success)return status;
            }
        }
        if(host.finish()!=DecodeResult::Success)return DecodeResult::Unsupported;
        if(staged.copy && !staged.writes.empty())return DecodeResult::Unsupported; // No staged source-read forwarding yet.
        staged.state.reference=host.state();output=std::move(staged);return DecodeResult::Success;
    }catch(const std::bad_alloc&){return DecodeResult::OutOfResource;}
}
}
