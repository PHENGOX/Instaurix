#include "machine.hpp"
#include "native_window.hpp"
#include <iostream>
#include <fstream>

struct Captured { uint64_t time,instructions; std::shared_ptr<const Frame> frame; uint32_t hash; };
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    auto drive=[&](Machine& machine,uint64_t chunk) {
        std::vector<Captured> trace;
        uint64_t total=0;
        for(unsigned i=0;i<20000;++i) {
            auto result=machine.run(chunk,true); total+=result.instructions;
            if(result.stop==Machine::Stop::Frame) {
                const auto frame=machine.process().frame();
                uint32_t hash=2166136261U;for(auto b:frame->rgb) hash=(hash^b)*16777619U;
                trace.push_back({machine.process().virtualTime(),total,frame,hash});
                check(trace.size()<=8 && result.instructions>0,"frame boundary delivered once without skipped frames");
            }
            else if(result.stop==Machine::Stop::Waiting) {
                const auto delay=machine.process().nextWakeDelay();
                check(bool(delay) && *delay==100000000,"guest sleep controls exact virtual frame interval");
                if(!delay || !machine.advanceTime(*delay)) break;
            }
            else if(result.stop==Machine::Stop::Exited) {
                check(machine.ram.read64(0x12000)==1 && machine.ram.read64(0x12010)==8 &&
                    machine.process().frameCount()==8 && machine.process().virtualTime()==700000000,"animation client completes eight frames at expected virtual time");
                return std::pair(trace,total);
            }
            else if(result.stop!=Machine::Stop::Budget) { check(false,"animation remains executable");break; }
        }
        check(false,"animation driver terminates");return std::pair(trace,total);
    };
    Machine continuous(false,true), sliced(false,true);
    check(continuous.mountReadOnlyRoot(argv[2]) && sliced.mountReadOnlyRoot(argv[2]) &&
        continuous.load(argv[1]) && sliced.load(argv[1]),"load animated resource client");
    auto [frames,total]=drive(continuous,10000000);
    auto [parts,slicedTotal]=drive(sliced,997);
    check(total==slicedTotal && frames.size()==8 && parts.size()==8,"same instruction total and all frames under budget slicing");
    std::ifstream file(std::filesystem::path(argv[2])/"sprite.txt",std::ios::binary);
    std::string sprite((std::istreambuf_iterator<char>(file)),{});
    if(sprite.size()!=272 || frames.size()!=8 || parts.size()!=8) return 1;
    for(size_t index=0;index<8;++index) {
        const auto& captured=frames[index];
        check(captured.time==index*100000000 && captured.time==parts[index].time &&
            captured.instructions==parts[index].instructions && captured.frame->rgb==parts[index].frame->rgb,
            "frame time instruction boundary and pixels invariant under run slicing");
        const auto& frame=*captured.frame;
        check(frame.width==160 && frame.height==96,"constant animation dimensions");
        const unsigned left=8+unsigned(index)*12;
        for(unsigned y=0;y<96;++y) for(unsigned x=0;x<160;++x) {
            uint8_t r=uint8_t(16+x/4),g=uint8_t(24+y/3),b=uint8_t(48+(x+y)/6);
            if((x*13+y*7+index*11)%113==0) { r=140;g=170;b=210; }
            if(x>=left && x<left+64 && y>=16 && y<80 && sprite[(y-16)/4*17+(x-left)/4]=='#') {
                r=uint8_t(240-(y-16));g=uint8_t(120+x-left);b=uint8_t(40+(y-16)*2);
            }
            const auto at=(y*160+x)*3;
            check(frame.rgb[at]==r && frame.rgb[at+1]==g && frame.rgb[at+2]==b,"all animated pixels match host reference");
        }
        if(index) check(captured.hash!=frames[index-1].hash,"successive frame content differs");
    }
    check(continuous.run(10,true).instructions==0,"completed stream does not replay final present");
    check(continuous.load(argv[1]) && continuous.process().frameCount()==0,"reload resets stream state");
#ifdef _WIN32
    NativeWindow viewer;
    check(viewer.open(false) && viewer.poll(),"native window created hidden for lifecycle test");
    check(viewer.present(frames.front().frame) && viewer.present(frames.back().frame) && viewer.waitFor(0),"window accepts successive frames and pumps messages");
    auto bad=std::make_shared<Frame>();check(!viewer.present(bad),"window rejects invalid frame");
    viewer.close();check(!viewer.poll() && !viewer.present(frames.front().frame),"window closure stops presentation");
    check(viewer.open(false),"window can be reopened after close");viewer.close();
#endif
    std::cout<<"Animation instructions="<<total<<" frames="<<frames.size()<<" failures="<<failures<<'\n';
    for(size_t i=0;i<frames.size();++i) std::cout<<i+1<<','<<frames[i].time<<','<<frames[i].instructions<<','<<frames[i].hash<<'\n';
    return failures?1:0;
}
