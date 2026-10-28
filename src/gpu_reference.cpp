#include "gpu_reference.hpp"
#include <new>
#include <utility>
namespace Gpu {
DecodeResult ReferenceInterpreter::apply(const MethodWrite& method,std::vector<ReferenceMemoryWrite>& writes){
    if(method.subchannel!=0)return DecodeResult::Unsupported;
    switch(method.method){
    case 0x50:reference.value=method.value;reference.written=true;break;
    case 0x10:if(method.value&~0xFFU)return DecodeResult::Malformed;upper=method.value;configured|=1;dirty=true;break;
    case 0x14:if(method.value&3)return DecodeResult::Malformed;lower=method.value;configured|=2;dirty=true;break;
    case 0x18:payload=method.value;configured|=4;dirty=true;break;
    case 0x1C:{
        // B06F RELEASE, 4BYTE, WFI enabled; acquire/reduction/16-byte forms remain unsupported.
        if(method.value!=0x01000002)return DecodeResult::Unsupported;
        if(configured!=7)return DecodeResult::Malformed;
        if(writes.size()==ReferenceWriteLimit)return DecodeResult::TransferLimit;
        ReferenceMemoryWrite write{VirtualAddress{(uint64_t(upper)<<32)|lower}};
        for(unsigned i=0;i<4;++i)write.bytes[i]=uint8_t(payload>>(i*8));
        try{writes.push_back(write);}catch(const std::bad_alloc&){return DecodeResult::OutOfResource;}
        dirty=false;break;
    }
    default:return DecodeResult::Unsupported;
    }
    return DecodeResult::Success;
}
DecodeResult prepareReferenceExecution(const SubmissionSnapshot& input,const ReferenceState& state,ReferenceExecution& output){
    DecodedPushbuffer decoded;const auto result=decodePushbuffer(input,decoded);
    if(result!=DecodeResult::Success)return result;
    try {
        ReferenceExecution staged;ReferenceInterpreter interpreter(state);
        for(const auto& method:decoded.methods){const auto status=interpreter.apply(method,staged.writes);if(status!=DecodeResult::Success)return status;}
        if(interpreter.finish()!=DecodeResult::Success)return DecodeResult::Unsupported;
        staged.state=interpreter.state();output=std::move(staged);return DecodeResult::Success;
    }catch(const std::bad_alloc&){return DecodeResult::OutOfResource;}
}
DecodeResult executeReferenceStream(const SubmissionSnapshot& input,ReferenceState& state){
    ReferenceExecution execution;const auto result=prepareReferenceExecution(input,state,execution);
    if(result!=DecodeResult::Success)return result;
    if(!execution.writes.empty())return DecodeResult::Unsupported;
    state=execution.state;return DecodeResult::Success;
}
}