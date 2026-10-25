#include "machine.hpp"
#include "native_window.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4)
{ for(size_t i=0;i<n;++i) b[at+i]=uint8_t(v>>(8*i)); }
struct Sample { uint64_t time,instructions,x,y; std::shared_ptr<const Frame> frame; };
int main(int argc,char** argv)
{
    if(argc!=3) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    InputState keys;
    check(keys.key(0x27,true) && keys.key(0x27,true) && keys.buttons()==2,"key repeat does not toggle held state");
    keys.key(0x25,true);check(keys.buttons()==3,"simultaneous opposite directions retained");
    keys.key(0x27,false);check(keys.buttons()==1,"key release clears only its button");
    check(!keys.key('A',true) && keys.buttons()==1,"unmapped key has no effect");
    keys.clear();check(keys.buttons()==0,"focus clear releases every key");
#ifdef _WIN32
    NativeWindow viewer;check(viewer.open(false),"create hidden keyboard window");
    HWND own=nullptr;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM parameter)->BOOL {
        wchar_t name[128]{};GetClassNameW(window,name,128);
        if(std::wstring(name)==L"Switch2EmuDevelopmentFrame") { *reinterpret_cast<HWND*>(parameter)=window;return FALSE; }
        return TRUE;
    },reinterpret_cast<LPARAM>(&own));
    check(own!=nullptr,"find only this test thread's own window");
    if(own) {
        SendMessageW(own,WM_KEYDOWN,0x27,0);SendMessageW(own,WM_KEYDOWN,0x27,0);
        check(viewer.buttons()==2,"window key down reaches input state");
        SendMessageW(own,WM_KEYDOWN,0x26,0);check(viewer.buttons()==6,"window diagonal keys combine");
        SendMessageW(own,WM_KEYUP,0x27,0);check(viewer.buttons()==4,"window key up reaches input state");
        SendMessageW(own,WM_KILLFOCUS,0,0);check(viewer.buttons()==0,"focus loss clears held keys");
        SendMessageW(own,WM_KEYDOWN,0x1B,0);check(viewer.buttons()==16,"window Escape maps to quit button");
        SendMessageW(own,WM_KEYUP,0x1B,0);check(viewer.buttons()==0 && viewer.quitRequested(),"quick Escape tap preserves quit request until guest observes it");
        SendMessageW(own,WM_CLOSE,0,0);check(!viewer.poll() && viewer.buttons()==0,"native close clears keyboard state");
    }
#endif
    std::ifstream resource(std::filesystem::path(argv[2])/"sprite.txt",std::ios::binary);
    std::string sprite((std::istreambuf_iterator<char>(resource)),{});if(sprite.size()!=272) return 2;
    auto drive=[&](const std::vector<uint32_t>& buttons,uint64_t chunk,bool verifyPixels) {
        Machine machine(false,true);
        check(machine.mountReadOnlyRoot(argv[2]) && machine.load(argv[1]),"load interactive client");
        machine.ram.write64(0x12030,buttons.size());
        std::vector<Sample> trace;uint64_t total=0;unsigned x=48,y=16;
        for(unsigned iteration=0;iteration<30000;++iteration) {
            const auto index=std::min(trace.size(),buttons.size()-1);
            check(machine.setButtons(buttons[index]),"inject valid input mask");
            auto result=machine.run(chunk,true);total+=result.instructions;
            if(result.stop==Machine::Stop::Frame) {
                if(trace.size()>=buttons.size()) { check(false,"no unexpected extra input frame");break; }
                const auto mask=buttons[trace.size()];
                if((mask&3)==1 && x>=4) x-=4;
                if((mask&3)==2 && x<=92) x+=4;
                if((mask&12)==4 && y>=4) y-=4;
                if((mask&12)==8 && y<=28) y+=4;
                auto frame=machine.process().frame();
                check(machine.ram.read64(0x12020)==x && machine.ram.read64(0x12028)==y,"guest movement and edge clamping match reference");
                trace.push_back({machine.process().virtualTime(),total,x,y,frame});
                if(verifyPixels) {
                    const auto phase=trace.size()-1;
                    for(unsigned py=0;py<96;++py) for(unsigned px=0;px<160;++px) {
                        uint8_t r=uint8_t(16+px/4),g=uint8_t(24+py/3),b=uint8_t(48+(px+py)/6);
                        if((px*13+py*7+phase*11)%113==0) { r=140;g=170;b=210; }
                        if(px>=x && px<x+64 && py>=y && py<y+64 && sprite[(py-y)/4*17+(px-x)/4]=='#') {
                            r=uint8_t(240-(py-y));g=uint8_t(120+px-x);b=uint8_t(40+(py-y)*2);
                        }
                        auto at=(py*160+px)*3;
                        check(frame->rgb[at]==r && frame->rgb[at+1]==g && frame->rgb[at+2]==b,"interactive pixels match scripted input and resource");
                    }
                }
            }
            else if(result.stop==Machine::Stop::Waiting) {
                auto delay=machine.process().nextWakeDelay();if(!delay || !machine.advanceTime(*delay)) break;
            }
            else if(result.stop==Machine::Stop::Exited) {
                check(machine.ram.read64(0x12000)==1 && trace.size()==buttons.size(),"interactive client completes scripted frame count");
                check(!machine.setButtons(0),"terminal machine refuses new input");
                std::cout<<"Input sequence frames="<<trace.size()<<" instructions="<<total<<'\n';
                return trace;
            }
            else if(result.stop!=Machine::Stop::Budget) break;
        }
        check(false,"interactive run terminates");return trace;
    };
    const std::vector<uint32_t> sequence{2,10,5,15,1,4,8,0};
    const auto whole=drive(sequence,10000000,true), sliced=drive(sequence,4093,false);
    check(whole.size()==8 && sliced.size()==8,"scripted interactive sequence complete");
    for(size_t i=0;i<std::min(whole.size(),sliced.size());++i)
        check(whole[i].time==i*100000000 && whole[i].instructions==sliced[i].instructions && whole[i].frame->rgb==sliced[i].frame->rgb,
            "input replay invariant under execution slicing");
    const auto lower=drive(std::vector<uint32_t>(14,10),10000000,false);
    const auto upper=drive(std::vector<uint32_t>(14,5),10000000,false);
    check(!lower.empty() && lower.back().x==96 && lower.back().y==32,"right and bottom boundaries stable under held input");
    check(!upper.empty() && upper.back().x==0 && upper.back().y==0,"left and top boundaries stable under held input");
    Machine quit(false,true);
    check(quit.mountReadOnlyRoot(argv[2]) && quit.load(argv[1]) && quit.setButtons(16),"inject guest quit before first frame");
    check(quit.run(100000).stop==Machine::Stop::Exited && quit.ram.read64(0x12000)==1 && quit.process().frameCount()==0,"Escape exits cleanly without presenting a fake frame");
    Machine raw;check(!raw.setButtons(1),"input injection requires HLE mode");

    Memory ram(Machine::RAM_SIZE);Process p;ProgramLoader::Image image;std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"input IPC fixture");
    Bus bus(ram);CPU cpu(bus);const auto tls=p.mainThread().localRegion;
    ram.setPermissions(0x30000,4096,Memory::Read|Memory::Write);
    const uint8_t port[]{'s','m',':',0};ram.writeBytes(0x30000,port,sizeof port);
    cpu.writeRegister(1,0x30000);p.dispatch(0x1F,cpu,bus);const auto sm=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t h) { ram.writeBytes(tls,b.data(),b.size());cpu.writeRegister(0,h);p.dispatch(0x21,cpu,bus);return uint32_t(cpu.readRegister(0)); };
    Ipc::Buffer init{};put(init,0,4);put(init,4,0x8000000A);put(init,8,1);put(init,32,0x49434653);send(init,sm);
    Ipc::Buffer get{};put(get,0,4);put(get,4,10);put(get,16,0x49434653);put(get,24,1);
    const char name[8]{'e','m','u',':','p','a','d',0};for(size_t i=0;i<8;++i) get[32+i]=name[i];
    check(send(get,sm)==0 && ram.read32(tls+24)==0,"lookup input service without file mount");
    const auto pad=ram.read32(tls+12);
    Ipc::Buffer read{};put(read,0,4);put(read,4,8);put(read,16,0x49434653);
    check(send(read,pad)==0 && ram.read64(tls+32)==0,"input defaults to released");
    check(p.setButtons(31) && !p.setButtons(32) && send(read,pad)==0 && ram.read64(tls+32)==31,"invalid input mask rejected transactionally");
    auto bad=read;put(bad,24,1);check(send(bad,pad)==0 && ram.read32(tls+24)==Ipc::UnknownCommand,"unknown input command explicit error");
    bad=read;put(bad,4,9);check(send(bad,pad)==0 && ram.read32(tls+24)==Ipc::InvalidArguments,"input payload not silently ignored");
    check(p.initialize(ram,image,true,error),"reset input process");
    cpu.writeRegister(1,0x30000);p.dispatch(0x1F,cpu,bus);
    const auto resetSm=uint32_t(cpu.readRegister(1));send(init,resetSm);send(get,resetSm);
    const auto resetPad=ram.read32(tls+12);
    check(send(read,resetPad)==0 && ram.read64(tls+32)==0,"process reset releases all injected buttons");
    std::cout<<"Interactive failures="<<failures<<'\n';return failures?1:0;
}
