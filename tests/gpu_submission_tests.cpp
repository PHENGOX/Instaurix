#include "gpu_submission.hpp"
#include <array>
#include <iostream>
static void put(std::span<uint8_t> bytes,size_t at,uint64_t value,size_t count=8){for(size_t i=0;i<count;++i)bytes[at+i]=uint8_t(value>>(8*i));}
int main(){
    using R=Gpu::PreflightResult;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    const uint64_t base=Gpu::AddressSpace::Base;std::array<uint8_t,24> header{};std::vector<uint8_t> entries(8),backing(0x20000,0x5A);put(header,8,1,4);put(header,12,4,4);put(entries,0,base|(2ULL<<42));
    unsigned reads=0;Gpu::CommandReader reader=[&](Gpu::VirtualAddress va,std::span<uint8_t> out){++reads;if(va.value<base || va.value-base>backing.size() || out.size()>backing.size()-size_t(va.value-base))return false;for(size_t i=0;i<out.size();++i)out[i]=backing[size_t(va.value-base)+i];return true;};
    Gpu::SubmissionSnapshot output;check(Gpu::prepareKickoff(header,entries,reader,output)==R::Success && output.flags==4 && output.buffers.size()==1 && output.buffers[0].address.value==base && output.buffers[0].bytes.size()==8 && output.buffers[0].bytes[0]==0x5A,"preflight resolves an aligned opaque command range");
    backing[0]=0xA5;put(entries,0,0);check(output.buffers[0].bytes[0]==0x5A && output.descriptors[0]==(base|(2ULL<<42)),"prepared command words and descriptors are owned copies");put(entries,0,base|(2ULL<<42));
    auto reject=[&](R expected){output.flags=99;output.descriptors={123};output.buffers={Gpu::CommandBufferSnapshot{Gpu::VirtualAddress{17},{42}}};check(Gpu::prepareKickoff(header,entries,reader,output)==expected,"invalid preflight returns its bounded result");check(output.flags==99 && output.descriptors.size()==1 && output.descriptors[0]==123 && output.buffers.size()==1 && output.buffers[0].address.value==17 && output.buffers[0].bytes==std::vector<uint8_t>{42},"failed preflight preserves the complete previously published snapshot");};
    for(const auto count:{0U,2U,2049U}){put(header,8,count,4);reject(R::InvalidArguments);}put(header,8,1,4);
    for(const auto flags:{0U,5U,260U}){put(header,12,flags,4);reject(R::InvalidArguments);}put(header,12,4,4);
    for(const auto offset:{16U,20U}){put(header,offset,1,4);reject(R::InvalidArguments);put(header,offset,0,4);}
    put(header,0,UINT64_MAX);check(Gpu::prepareKickoff(header,entries,reader,output)==R::Success,"ignored parameter pointer is not dereferenced");
    for(const uint64_t descriptor:std::array<uint64_t,5>{base,base|(1ULL<<40)|(1ULL<<42),base|(1ULL<<41)|(1ULL<<42),(base+1)|(1ULL<<42),base|(1ULL<<42)|(1ULL<<63)}){put(entries,0,descriptor);reject(R::InvalidArguments);}
    for(const uint64_t address:{uint64_t(0x200000),base+Gpu::AddressSpace::Capacity}){put(entries,0,address|(1ULL<<42));reject(R::InvalidAddress);}
    put(entries,0,(base+Gpu::AddressSpace::Capacity-4)|(2ULL<<42));reject(R::InvalidAddress);
    put(entries,0,base|(16385ULL<<42));reads=0;reject(R::TransferLimit);check(!reads,"aggregate limits are enforced before reading any command data");
    put(entries,0,base|(16384ULL<<42));check(Gpu::prepareKickoff(header,entries,reader,output)==R::Success && output.buffers[0].bytes.size()==0x10000,"one command range may reach the local 64 KiB snapshot limit");
    put(header,8,2,4);entries.resize(16);put(entries,0,base|(8192ULL<<42));put(entries,8,(base+0x10000)|(8192ULL<<42));check(Gpu::prepareKickoff(header,entries,reader,output)==R::Success && output.buffers.size()==2,"two ranges may share the aggregate limit");put(entries,8,(base+0x10000)|(8193ULL<<42));reads=0;reject(R::TransferLimit);check(!reads,"aggregate limit checks every descriptor before copying");
    put(entries,0,base|(1ULL<<42));put(entries,8,(base+0x20000)|(1ULL<<42));reject(R::InvalidAddress);
    put(header,8,2048,4);entries.resize(2048*8);for(size_t i=0;i<2048;++i)put(entries,i*8,base|(1ULL<<42));check(Gpu::prepareKickoff(header,entries,reader,output)==R::Success && output.buffers.size()==2048,"the local channel descriptor capacity is bounded and supported");
    put(header,8,1,4);entries.resize(7);reject(R::InvalidArguments);entries.resize(8);put(entries,0,base|(1ULL<<42));
    check(Gpu::prepareKickoff(std::span<const uint8_t>(header).first(23),entries,reader,output)==R::InvalidArguments && Gpu::prepareKickoff(header,entries,{},output)==R::InvalidArguments,"parameter extent and reader capability are required");
    std::cout<<"GPU submission preflight failures="<<failures<<'\n';return failures?1:0;
}
