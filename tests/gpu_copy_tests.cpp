#include "gpu_channel.hpp"
#include <iostream>
#include <algorithm>
static void append(Gpu::SubmissionSnapshot& input,uint32_t value){if(input.buffers.empty())input.buffers.emplace_back();for(unsigned i=0;i<4;++i)input.buffers.back().bytes.push_back(uint8_t(value>>(8*i)));}
static Gpu::SubmissionSnapshot copy(uint64_t source=0x100001000ULL,uint64_t destination=0x100003000ULL,uint32_t size=64,uint32_t flags=0x186){
    Gpu::SubmissionSnapshot input;for(const auto value:{0x60018000U,0xB0B5U,0x20048100U,uint32_t(source>>32),uint32_t(source),uint32_t(destination>>32),uint32_t(destination),0x20018106U,size,0x200180C0U,flags})append(input,value);return input;
}
int main(){
    using R=Gpu::DecodeResult;unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Gpu::ChannelState state;Gpu::ChannelExecution output;auto input=copy();
    check(Gpu::findClass(0xB0B5)->support==Gpu::ClassSupport::BoundedCopy && Gpu::findClass(0xB0B5)->handler,"Maxwell copy has its own bounded handler");
    check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy && output.copy->source.value==0x100001000ULL && output.copy->destination.value==0x100003000ULL && output.copy->bytes==64 && output.state.bindings[4]==0xB0B5 && output.state.copy[4].configured==31 && !state.bindings[4],"field-derived DMA stream stages one 64-byte transfer and register bank");
    state=output.state;Gpu::SubmissionSnapshot launch;append(launch,0x818680C0);
    check(Gpu::prepareChannelExecution(launch,state,output)==R::Success && output.copy && output.copy->bytes==64,"later submission reuses committed registers with an immediate launch");
    input=copy();append(input,0x20018106);append(input,99);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->bytes==64 && output.state.copy[4].length==99,"post-launch register updates cannot retroactively change a captured transfer");
    auto reject=[&](Gpu::SubmissionSnapshot bad,R expected){output.state=state;output.state.reference={99,true};output.copy=Gpu::CopyRequest{Gpu::VirtualAddress{1},Gpu::VirtualAddress{2},3};output.writes={{Gpu::VirtualAddress{4},{1,2,3,4}}};const auto before=output.state;check(Gpu::prepareChannelExecution(bad,state,output)==expected && output.state==before && output.copy && output.copy->source.value==1 && output.copy->bytes==3 && output.writes.size()==1,"rejected copy preserves the complete previous execution plan");};
    reject(copy(0x100001000ULL,0x100001000ULL),R::Malformed);reject(copy(0x100001000ULL,0x10000103FULL),R::Malformed);
    reject(copy(0x100001000ULL,0x100003000ULL,0),R::Malformed);reject(copy(0x100001000ULL,0x100003000ULL,Gpu::CopyByteLimit+1),R::TransferLimit);
    reject(copy(0x10000000000ULL),R::Malformed);
    for(const auto flags:{0U,0x182U,0x187U,0x586U,0x1186U})reject(copy(0x100001000ULL,0x100003000ULL,64,flags),R::Unsupported);
    reject(copy(0x100001000ULL,0x100003000ULL,64,0x386),R::Malformed);
    input=copy();append(input,0x818680C0);reject(input,R::TransferLimit);
    input=copy();append(input,0x800080C1);reject(input,R::Unsupported);
    input=copy();append(input,0x20010014);reject(input,R::Malformed);
    input=copy();for(const auto value:{0x20040004U,1U,0x8000U,0x99U,0x01000002U})append(input,value);reject(input,R::Unsupported);
    input={};append(input,0x60018000);append(input,0xB0B5);append(input,0x818680C0);reject(input,R::Malformed);
    input={};append(input,0x60018000);append(input,0xB197);append(input,0x818680C0);reject(input,R::Unsupported);
    Gpu::ChannelState unbound;check(Gpu::prepareChannelExecution(launch,unbound,output)==R::Unsupported,"unbound channel cannot reuse another channel's copy registers");
    input=copy(0x100001001ULL,0x100003003ULL,63);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->bytes==63,"byte-addressed linear copy does not invent word alignment requirements");
    input=copy(0x100001000ULL,0x100001040ULL,64);check(Gpu::prepareChannelExecution(input,state,output)==R::Success,"adjacent disjoint ranges are accepted exactly");
    input=copy(0x100001000ULL,0x100020000ULL,Gpu::CopyByteLimit);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->bytes==Gpu::CopyByteLimit,"local transfer limit is accepted exactly before mapping validation");
    auto rectangle=[&](uint32_t width,uint32_t rows,uint32_t sourcePitch,uint32_t destinationPitch,uint64_t source=0x100001001ULL,uint64_t destination=0x100020003ULL){
        auto stream=copy(source,destination,width);stream.buffers[0].bytes.resize(9*4);
        for(const auto word:{0x20028104U,sourcePitch,destinationPitch,0x20018107U,rows,0x200180C0U,0x386U})append(stream,word);return stream;
    };
    input=rectangle(7,3,13,19);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->rows==3 && output.copy->sourcePitch==13 && output.copy->destinationPitch==19 && output.copy->bytes==7 && output.state.copy[4].configured==255,"pitched copy captures width, distinct row strides and count");
    const auto rectangularState=output.state;append(input,0x20018107);append(input,1);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->rows==3 && output.state.copy[4].rows==1,"post-launch row changes do not affect captured geometry");
    Gpu::SubmissionSnapshot pitchedLaunch;append(pitchedLaunch,0x838680C0);check(Gpu::prepareChannelExecution(pitchedLaunch,rectangularState,output)==R::Success && output.copy->rows==3,"multiline geometry persists across submissions");
    check(Gpu::prepareChannelExecution(launch,rectangularState,output)==R::Success && output.copy->rows==1 && output.copy->sourcePitch==7,"single-line mode ignores saved pitch and count registers");
    for(const auto dimensions:std::array<std::array<uint32_t,4>,5>{{{7,0,13,19},{7,3,0,19},{7,3,13,6},{7,3,6,19},{0,3,13,19}}})reject(rectangle(dimensions[0],dimensions[1],dimensions[2],dimensions[3]),R::Malformed);
    reject(rectangle(7,Gpu::CopyRowLimit+1,13,19),R::TransferLimit);
    reject(rectangle(1025,64,1025,1025),R::TransferLimit);
    reject(rectangle(1,2,UINT32_MAX,1),R::TransferLimit);
    reject(rectangle(1,UINT32_MAX,UINT32_MAX,UINT32_MAX),R::TransferLimit);
    reject(rectangle(7,3,13,19,(1ULL<<40)-20),R::Malformed);
    reject(rectangle(7,3,13,19,0x100001000ULL,0x100001020ULL),R::Malformed);
    input=rectangle(1024,64,1024,1024);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->rows==64,"row and byte limits are accepted exactly");
    auto block=[&](uint32_t width=128,uint32_t rows=16,uint32_t pitch=144,uint64_t source=0x100001000ULL){
        auto stream=rectangle(width,rows,0,pitch,source);auto& bytes=stream.buffers[0].bytes;bytes.resize(bytes.size()-8);
        for(const auto word:{0x200681CAU,0x1000U,width,rows,1U,0U,0U,0x200180C0U,0x306U})append(stream,word);return stream;
    };
    auto change=[&](Gpu::SubmissionSnapshot stream,unsigned fromEnd,uint32_t value){auto& b=stream.buffers[0].bytes;const auto at=b.size()-fromEnd*4;for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(value>>(i*8));return stream;};
    input=block();check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->sourceBlockLinear && output.copy->bytes==128 && output.copy->rows==16 && output.state.copy[4].sourceBlockConfigured==63,"complete one-GOB-height source stages a block-to-pitch transfer");
    const auto blockState=output.state;Gpu::SubmissionSnapshot blockLaunch;append(blockLaunch,0x830680C0);
    check(Gpu::prepareChannelExecution(blockLaunch,blockState,output)==R::Success && output.copy->sourceBlockLinear,"block geometry persists across successful submissions");
    input=block();append(input,0x200181CB);append(input,64);check(Gpu::prepareChannelExecution(input,state,output)==R::Success && output.copy->bytes==128 && output.state.copy[4].sourceWidth==64,"post-launch surface changes do not alter the captured plan");
    reject(block(128,16,144,0x100001001ULL),R::Malformed);
    reject(block(65,16,144),R::Unsupported);reject(block(128,9,144),R::Unsupported);
    reject(change(block(),8,0x1010),R::Unsupported); // Two GOBs high.
    reject(change(block(),8,0),R::Unsupported); // Tesla GOB layout.
    reject(change(block(),5,2),R::Unsupported); // Depth.
    reject(change(block(),4,1),R::Unsupported); // Layer.
    reject(change(block(),3,1),R::Unsupported); // Origin.
    reject(change(block(),7,64),R::Unsupported);reject(change(block(),6,8),R::Unsupported);
    reject(change(block(),1,0x286),R::Unsupported); // Reverse direction is not yet implemented.
    reject(change(rectangle(128,16,0,144),1,0x306),R::Malformed);
    reject(block(2048,64,2048),R::TransferLimit);
    input=block(1024,64,1024);check(Gpu::prepareChannelExecution(input,state,output)==R::Success,"complete block image accepts the exact 64-KiB limit");
    std::array<bool,2048> visited{};bool unique=true;
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<128;++x){const auto offset=Gpu::copyBlockSourceOffset(128,x,y);if(offset>=visited.size() || visited[offset])unique=false;else visited[offset]=true;}
    check(unique && std::all_of(visited.begin(),visited.end(),[](bool v){return v;}),"multi-GOB addressing is a full byte permutation");
    check(Gpu::copyBlockSourceOffset(128,16,0)==32 && Gpu::copyBlockSourceOffset(128,32,0)==256 && Gpu::copyBlockSourceOffset(128,0,1)==16 && Gpu::copyBlockSourceOffset(128,64,0)==512 && Gpu::copyBlockSourceOffset(128,0,8)==1024,"known sector and GOB boundary offsets match Fermi 16Bx2 ordering");
    std::cout<<"Maxwell bounded copy plan failures="<<failures<<'\n';return failures?1:0;
}
