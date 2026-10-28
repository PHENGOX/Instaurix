#include "gpu_reference.hpp"
#include <initializer_list>
#include <iostream>
static Gpu::CommandBufferSnapshot buffer(std::initializer_list<uint32_t> words){
    Gpu::CommandBufferSnapshot out{};
    for(const auto word:words)for(unsigned i=0;i<4;++i)out.bytes.push_back(uint8_t(word>>(i*8)));
    return out;
}
int main(){
    using R=Gpu::DecodeResult;unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Gpu::ReferenceState state;Gpu::SubmissionSnapshot input;
    input.buffers={buffer({0x20010014,0x12345678,0})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.written && state.value==0x12345678,"documented SET_REFERENCE writes a local reference register");
    input.buffers={buffer({0x60030014,0,0xFFFFFFFF,0x87654321})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.value==0x87654321,"non-incrementing writes execute in stream order with a full 32-bit payload");
    input.buffers={buffer({0x80000014})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.written && !state.value,"zero immediate value is a valid reference write");
    input.buffers={buffer({0xA0010014,0xFFFFFFFF})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.value==0xFFFFFFFF,"single increment-once packet executes its supported method");
    input.buffers={buffer({0,0})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.value==0xFFFFFFFF,"DMA_NOP leaves existing reference state unchanged");
    Gpu::ReferenceState other;
    check(Gpu::executeReferenceStream(input,other)==R::Success && !other.written && !other.value,"independent state does not inherit another register");
    input.buffers={buffer({0x20010014,1}),buffer({0x20010014,2})};
    check(Gpu::executeReferenceStream(input,state)==R::Success && state.value==2,"supported methods preserve order across command ranges");
    auto reject=[&](R expected,const char* text){const auto before=state;check(Gpu::executeReferenceStream(input,state)==expected && state.value==before.value && state.written==before.written,text);};
    input.buffers={buffer({0x20020014,3,4})};reject(R::Unsupported,"late unknown incremented method rolls back the reference write");
    input.buffers={buffer({0xA0020014,3,4})};reject(R::Unsupported,"increment-once successor is rejected without partial execution");
    input.buffers={buffer({0x20010014,3}),buffer({0x20012014,4})};reject(R::Unsupported,"nonzero subchannel is outside the local execution profile");
    input.buffers={buffer({0x20010014,3,0x20020014,4})};reject(R::Malformed,"late truncated packet cannot mutate persistent reference state");
    input.buffers={buffer({0x20010014,3,0xE0000000})};reject(R::Unsupported,"unsupported packet control cannot mutate reference state");
    input.buffers={Gpu::CommandBufferSnapshot{Gpu::VirtualAddress{},std::vector<uint8_t>(0x10004)}};reject(R::TransferLimit,"oversized input preserves state");
    input.buffers.clear();reject(R::Malformed,"empty submission is not successful execution");
    std::cout<<"GPU reference interpreter failures="<<failures<<'\n';return failures?1:0;
}
