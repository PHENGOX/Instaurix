#include "gpu_reference.hpp"
#include <iostream>
static void word(Gpu::CommandBufferSnapshot& buffer,uint32_t value){for(unsigned i=0;i<4;++i)buffer.bytes.push_back(uint8_t(value>>(8*i)));}
static Gpu::SubmissionSnapshot release(uint32_t upper=1,uint32_t lower=0x2000,uint32_t payload=0x78563412,uint32_t operation=0x01000002){
    Gpu::SubmissionSnapshot input;input.buffers.emplace_back();
    for(const auto value:{0x20040004U,upper,lower,payload,operation})word(input.buffers[0],value);
    return input;
}
int main(){
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    using R=Gpu::DecodeResult;const Gpu::ReferenceState initial{0x42,true};Gpu::ReferenceExecution output;
    auto input=release();
    check(Gpu::prepareReferenceExecution(input,initial,output)==R::Success && output.state.value==0x42 && output.writes.size()==1 && output.writes[0].address.value==0x100002000ULL && output.writes[0].bytes==std::array<uint8_t,4>{0x12,0x34,0x56,0x78},"NVIDIA field-derived four-byte release stages the exact little-endian payload");
    auto reject=[&](R expected){output.state={0xBEEF,true};output.writes={{Gpu::VirtualAddress{0x1234},{1,2,3,4}}};check(Gpu::prepareReferenceExecution(input,initial,output)==expected && output.state.value==0xBEEF && output.writes.size()==1 && output.writes[0].address.value==0x1234 && output.writes[0].bytes==std::array<uint8_t,4>{1,2,3,4},"rejected stream leaves the complete previous output plan intact");};
    input=release(0x100);reject(R::Malformed);input=release(1,3);reject(R::Malformed);
    for(const auto operation:{2U,0x01000001U,0x01100002U,0x81000002U}){input=release(1,0x2000,0,operation);reject(R::Unsupported);}
    input=release();input.buffers[0].bytes.resize(12);reject(R::Malformed);
    input.buffers.clear();input.buffers.emplace_back();word(input.buffers[0],0x20010007);word(input.buffers[0],0x01000002);reject(R::Malformed);
    input=release();word(input.buffers[0],0x20010004);word(input.buffers[0],1);reject(R::Unsupported);
    input=release();word(input.buffers[0],0xE0000000);reject(R::Unsupported);
    input=release(0xFF,0xFFFFFFFC,0xFFFFFFFF);check(Gpu::prepareReferenceExecution(input,initial,output)==R::Success && output.writes[0].address.value==0xFFFFFFFFFCULL && output.writes[0].bytes==std::array<uint8_t,4>{255,255,255,255},"maximum 40-bit aligned address and full payload are retained for driver validation");
    input=release(0,0,0);check(Gpu::prepareReferenceExecution(input,initial,output)==R::Success && !output.writes[0].address.value && output.writes[0].bytes==std::array<uint8_t,4>{},"zero fields are representable without implying a mapped address");
    auto state=initial;check(Gpu::executeReferenceStream(input,state)==R::Unsupported && state.value==initial.value,"reference-only helper never silently discards prepared memory effects");
    input=release();for(const auto value:{0x20010006U,0xAABBCCDDU,0x20010007U,0x01000002U,0x20010014U,0x99U})word(input.buffers[0],value);
    check(Gpu::prepareReferenceExecution(input,initial,output)==R::Success && output.writes.size()==2 && output.writes[0].bytes[0]==0x12 && output.writes[1].bytes[0]==0xDD && output.state.value==0x99,"ordered release writes and reference updates share one staged plan");
    input=release();const auto packet=input.buffers[0].bytes;
    for(size_t i=1;i<Gpu::ReferenceWriteLimit;++i)input.buffers[0].bytes.insert(input.buffers[0].bytes.end(),packet.begin(),packet.end());
    check(Gpu::prepareReferenceExecution(input,initial,output)==R::Success && output.writes.size()==Gpu::ReferenceWriteLimit,"local 256-write bound is accepted exactly");
    input.buffers[0].bytes.insert(input.buffers[0].bytes.end(),packet.begin(),packet.end());reject(R::TransferLimit);
    std::cout<<"GPU memory release plan failures="<<failures<<'\n';return failures?1:0;
}
