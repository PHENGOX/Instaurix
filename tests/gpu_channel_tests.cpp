#include "gpu_channel.hpp"
#include <initializer_list>
#include <iostream>
static Gpu::SubmissionSnapshot stream(std::initializer_list<uint32_t> words){Gpu::SubmissionSnapshot out;out.buffers.emplace_back();for(auto value:words)for(unsigned i=0;i<4;++i)out.buffers[0].bytes.push_back(uint8_t(value>>(8*i)));return out;}
int main(){
    using R=Gpu::DecodeResult;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    const auto registry=Gpu::classRegistry();check(registry.size()==6,"registry describes exact known classes without claiming all engines execute");
    for(size_t i=0;i<registry.size();++i){check(Gpu::findClass(registry[i].id)==&registry[i],"class lookup resolves its exact descriptor");for(size_t j=0;j<i;++j)check(registry[i].id!=registry[j].id,"class identities are unique");}
    check(!Gpu::findClass(0) && !Gpu::findClass(0xDEAD) && !Gpu::findProfile("switch2") && !Gpu::findProfile(static_cast<Gpu::ProfileId>(99)),"unknown classes and profiles never fall back silently");
    check(Gpu::profileAllows(Gpu::ProfileId::MaxwellReference,0xB0B5) && !Gpu::profileAllows(Gpu::ProfileId::MaxwellReference,0xC6B5) && Gpu::profileAllows(Gpu::ProfileId::AmpereResearch,0xC6B5) && Gpu::profileAllows(Gpu::ProfileId::AmpereResearch,0xC7B5) && Gpu::profileAllows(Gpu::ProfileId::AmpereResearch,0xC797),"profiles authorize separate hardware classes");
    for(const auto id:{0xC6B5U,0xC7B5U,0xC797U})check(Gpu::findClass(id)->support==Gpu::ClassSupport::DeclaredOnly && !Gpu::findClass(id)->handler,"known unimplemented classes have no execution capability or fallback handler");
    check(Gpu::findClass(0xB197)->handler && !Gpu::findClass(0xB06F)->handler,"engine handlers and channel control are separate dispatch paths");
    Gpu::ChannelState state;Gpu::ChannelExecution output;
    auto input=stream({0x60010000,0xB197,0x80000040,0x20010014,0x12345678});
    check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.state.bindings[0]==0xB197 && output.state.reference==Gpu::ReferenceState{0x12345678,true} && state.bindings[0]==0,"binding plus limited 3D noop and global host method are staged together");
    state=output.state;input=stream({0x80000040});check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.state==state,"class binding persists across prepared submissions");
    input=stream({0x60018000,0xB197,0x80008040});check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.state.bindings[4]==0xB197 && output.state.bindings[0]==0xB197,"subchannel bindings are independently selected in the local profile");
    Gpu::ChannelState other;input=stream({0x80000040});check(Gpu::prepareChannelExecution(input,other,output)==R::Unsupported && !other.bindings[0],"unbound engine commands do not inherit another channel's class");
    auto reject=[&](Gpu::SubmissionSnapshot request,R expected){output.state=state;output.state.reference={0xBEEF,true};output.writes={{Gpu::VirtualAddress{0x1000},{1,2,3,4}}};const auto before=output.state;check(Gpu::prepareChannelExecution(request,state,output)==expected && output.state==before && output.writes.size()==1 && output.writes[0].bytes==std::array<uint8_t,4>{1,2,3,4},"rejection preserves all prior output state and memory effects");};
    for(const auto id:{0U,0xDEADU,0xC6B5U,0xC7B5U,0xC797U,0xB06FU,0x1B197U})reject(stream({0x60010000,id}),R::Unsupported);
    reject(stream({0x60018000,0xB197,0x80008101}),R::Unsupported); // Unknown engine method after staged binding.
    reject(stream({0x60018000,0xB197,0x20010014,99,0xE0000000}),R::Unsupported);
    reject(stream({0x60018000,0xB197,0x20010014}),R::Malformed);
    reject(stream({0x60010000,0xB197,0x20010040,1}),R::Unsupported); // Only the zero-argument noop is supported.
    input=stream({0x2001C014,0xAABBCCDD});check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.state.reference.value==0xAABBCCDD,"historical FIFO subchannel six reaches host control independently of engine bindings");
    reject(stream({0x20012014,1}),R::Unsupported);
    input=stream({0x60010000,0xB197,0x20040004,1,0x2000,0x78563412,0x01000002});
    check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.state.bindings[0]==0xB197 && output.writes.size()==1 && output.writes[0].address.value==0x100002000ULL,"engine selection composes with existing staged semaphore writes");
    Gpu::ChannelState ampere;ampere.profile=Gpu::ProfileId::AmpereResearch;output.state=state;
    check(Gpu::prepareChannelExecution(stream({0}),ampere,output)==R::Unsupported && output.state==state,"Ampere research cannot borrow the historical FIFO ABI even for a nop");
    auto invalid=state;invalid.bindings[1]=0xC797;check(Gpu::prepareChannelExecution(stream({0}),invalid,output)==R::Unsupported,"inconsistent persisted class state is refused");
    std::cout<<"Modular GPU channel failures="<<failures<<'\n';return failures?1:0;
}
