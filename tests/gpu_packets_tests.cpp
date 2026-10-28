#include "gpu_packets.hpp"
#include <iostream>
#include <initializer_list>
static Gpu::CommandBufferSnapshot buffer(std::initializer_list<uint32_t> words){Gpu::CommandBufferSnapshot out{};for(auto word:words)for(unsigned i=0;i<4;++i)out.bytes.push_back(uint8_t(word>>(i*8)));return out;}
int main(){
    using R=Gpu::DecodeResult;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Gpu::SubmissionSnapshot input;Gpu::DecodedPushbuffer output;
    input.buffers={buffer({0x20010014,0x12345678,0})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods.size()==1 && output.methods[0].method==0x50 && output.methods[0].value==0x12345678 && output.methods[0].subchannel==0 && output.dmaNops==1,"NVIDIA-derived SET_REFERENCE and DMA_NOP reference decodes independently of execution");
    input.buffers={buffer({0x20030014,0x11,0x22,0x33})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods.size()==3 && output.methods[0].method==0x50 && output.methods[1].method==0x54 && output.methods[2].method==0x58 && output.methods[2].value==0x33,"incrementing methods advance by four bytes");
    input.buffers={buffer({0x60032014,0x11,0x22,0x33})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods[0].subchannel==1 && output.methods[1].method==0x50 && output.methods[2].method==0x50,"non-incrementing methods retain their address and subchannel");
    input.buffers={buffer({0xA003E014,0x11,0x22,0x33})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods[0].subchannel==7 && output.methods[0].method==0x50 && output.methods[1].method==0x54 && output.methods[2].method==0x54,"one-increment format advances once only");
    input.buffers={buffer({0x92340014,0x80000014})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods.size()==2 && output.methods[0].value==0x1234 && output.methods[1].value==0,"immediate format carries its value without consuming following data");
    input.buffers={buffer({0,0x20010014,1}),buffer({0x20010FFF,0xDEADBEEF})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.dmaNops==1 && output.methods[0].buffer==0 && output.methods[0].headerWord==1 && output.methods[1].buffer==1 && output.methods[1].method==0x3FFC,"buffer identity, header offsets and last method address are retained");
    auto reject=[&](R expected){output.dmaNops=99;output.methods={{3,4,5,6,7}};check(Gpu::decodePushbuffer(input,output)==expected,"invalid command stream returns its bounded decode result");check(output.dmaNops==99 && output.methods.size()==1 && output.methods[0].buffer==3 && output.methods[0].headerWord==4 && output.methods[0].subchannel==5 && output.methods[0].method==6 && output.methods[0].value==7,"failed decoding never publishes a partial method trace");};
    for(auto header:{0x20000014U,0x20011014U,0x20020FFFU,0xA0020FFFU,0x20030014U}){input.buffers={buffer({header,1,2})};reject(R::Malformed);}
    input.buffers={buffer({0x20010014}),buffer({0x12345678})};reject(R::Malformed);
    for(auto header:{1U,0x40010014U,0xC0010014U,0xE0000000U}){input.buffers={buffer({0x20010014,0x12345678,header})};reject(R::Unsupported);}
    input.buffers={buffer({0x20010014,1,0x20020014,2})};reject(R::Malformed);
    input.buffers={buffer({0x60020FFF,1,2})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods[1].method==0x3FFC,"non-increment format does not falsely overflow the method range");
    input.buffers={buffer({0xA0010FFF,1})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods[0].method==0x3FFC,"one-increment with one payload does not increment past the range");
    input.buffers={buffer({0})};check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods.empty() && output.dmaNops==1,"exact zero word is a DMA no-operation packet");
    input.buffers.clear();reject(R::Malformed);input.buffers={buffer({})};reject(R::Malformed);input.buffers={buffer({0})};input.buffers[0].bytes.push_back(0);reject(R::Malformed);
    input.buffers={Gpu::CommandBufferSnapshot{Gpu::VirtualAddress{},std::vector<uint8_t>(0x10000)}};for(size_t i=0;i<input.buffers[0].bytes.size();i+=4){input.buffers[0].bytes[i]=0x14;input.buffers[0].bytes[i+3]=0x80;}check(Gpu::decodePushbuffer(input,output)==R::Success && output.methods.size()==0x4000,"bounded all-immediate stream reaches the aggregate word limit");input.buffers.push_back(buffer({0}));reject(R::TransferLimit);
    std::cout<<"GPU packet decoder failures="<<failures<<'\n';return failures?1:0;
}
