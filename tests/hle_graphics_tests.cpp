#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <iostream>

static void put(Ipc::Buffer& b,size_t at,uint64_t value,size_t n=4)
{ for(size_t i=0;i<n;++i) b[at+i]=uint8_t(value>>(8*i)); }
static Ipc::Buffer present(uint32_t width,uint32_t height,uint32_t source=0x31000,uint32_t size=3)
{
    Ipc::Buffer b{}; put(b,0,0x00100004); put(b,4,10); put(b,8,size); put(b,12,source);
    put(b,32,0x49434653); put(b,48,width); put(b,52,height); return b;
}
int main(int argc,char** argv)
{
    if(argc!=4) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true), sliced(false,true);
    check(m.mountReadOnlyRoot(argv[2]) && sliced.mountReadOnlyRoot(argv[2]) && m.load(argv[1]) && sliced.load(argv[1]),"mount sprite and load compiled graphics client");
    const auto whole=m.run(1000000);
    const auto frame=m.process().frame();
    check(whole.stop==Machine::Stop::Exited && m.ram.read64(0x12000)==1 && m.ram.read64(0x12008)==272 &&
        m.ram.read64(0x12010)==46080 && frame && frame->width==160 && frame->height==96 &&
        frame->rgb.size()==46080 && m.process().frameCount()==1,"guest renders resource into one complete frame");
    if(!frame) return 1;
    std::ifstream resource(std::filesystem::path(argv[2])/"sprite.txt",std::ios::binary);
    std::string sprite((std::istreambuf_iterator<char>(resource)),{});
    if(sprite.size()!=272) return 2;
    uint32_t hash=2166136261U;
    for(unsigned y=0;y<96;++y) for(unsigned x=0;x<160;++x) {
        uint8_t r=uint8_t(16+x/4),g=uint8_t(24+y/3),b=uint8_t(48+(x+y)/6);
        if((x*13+y*7)%113==0) { r=140;g=170;b=210; }
        if(x>=48 && x<112 && y>=16 && y<80 && sprite[(y-16)/4*17+(x-48)/4]=='#') {
            r=uint8_t(240-(y-16));g=uint8_t(120+(x-48));b=uint8_t(40+(y-16)*2);
        }
        const auto at=(y*160+x)*3;
        check(frame->rgb[at]==r && frame->rgb[at+1]==g && frame->rgb[at+2]==b,"all pixels match independent host rendering");
        for(auto c:{r,g,b}) hash=(hash^c)*16777619U;
    }
    check(m.ram.read64(0x12018)==hash,"guest and host frame hash agree");
    uint64_t count=0;
    for(unsigned i=0;i<10000;++i) {
        const auto part=sliced.run(127); count+=part.instructions;
        if(part.stop==Machine::Stop::Exited) break;
    }
    check(count==whole.instructions && sliced.process().frame() && sliced.process().frame()->rgb==frame->rgb,
        "image and instruction count invariant under sliced execution");
    std::string error;
    check(frame->savePng(std::filesystem::path(argv[3])/"graphics-verified.png",error),"export frame to PNG");
    Frame large; large.width=85; large.height=256; large.rgb.assign(85*256*3,90);
    check(large.savePng(std::filesystem::path(argv[3])/"graphics-two-blocks.png",error),"PNG export spans two DEFLATE blocks");
    Frame invalid;
    check(!invalid.savePng(std::filesystem::path(argv[3])/"invalid-frame.png",error),"invalid frame rejected before export");
    check(!m.load("missing-graphics-test.elf") && m.process().frame()==frame,"failed load preserves displayed frame");
    check(m.load(argv[1]) && !m.process().frame() && m.process().frameCount()==0,"successful reload clears last frame");

    Memory ram(Machine::RAM_SIZE); Process p; ProgramLoader::Image image;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"display dispatcher fixture");
    Bus bus(ram); CPU cpu(bus); const auto tls=p.mainThread().localRegion;
    ram.setPermissions(0x30000,0x2000,Memory::Read|Memory::Write);
    const uint8_t port[]{'s','m',':',0}; ram.writeBytes(0x30000,port,sizeof port);
    cpu.writeRegister(1,0x30000); p.dispatch(0x1F,cpu,bus); const auto sm=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t h) { ram.writeBytes(tls,b.data(),b.size()); cpu.writeRegister(0,h); p.dispatch(0x21,cpu,bus); return uint32_t(cpu.readRegister(0)); };
    auto init=Ipc::Buffer{}; put(init,0,4);put(init,4,0x8000000A);put(init,8,1);put(init,32,0x49434653);
    check(send(init,sm)==0,"initialize display service lookup");
    auto get=Ipc::Buffer{}; put(get,0,4);put(get,4,10);put(get,16,0x49434653);put(get,24,1);
    const char* name="emu:disp"; for(size_t i=0;i<8;++i) get[32+i]=uint8_t(name[i]);
    check(send(get,sm)==0 && ram.read32(tls+24)==0,"obtain local display service via moved handle");
    const auto display=ram.read32(tls+12);
    ram.write8(0x31000,1);ram.write8(0x31001,2);ram.write8(0x31002,3);
    check(send(present(1,1),display)==0 && ram.read32(tls+24)==0 && p.frame() && p.frame()->rgb==std::vector<uint8_t>({1,2,3}),"one-pixel RGB present");
    const auto first=p.frame();ram.write8(0x31000,9);
    check(first->rgb[0]==1,"present captures immutable source snapshot");
    for(auto b:{present(0,1),present(257,1),present(UINT32_MAX,UINT32_MAX),present(1,1,0x31000,2)})
        check(send(b,display)==0 && ram.read32(tls+24)==Ipc::InvalidArguments && p.frame()==first && p.frameCount()==1,"invalid dimensions or size retain last frame");
    auto b=present(1,1,Machine::DEVICE_BASE);
    check(send(b,display)==Ipc::InvalidBuffer && p.frame()==first,"MMIO source cannot become frame");
    b=present(1,1);put(b,40,99);
    check(send(b,display)==0 && ram.read32(tls+24)==Ipc::UnknownCommand && p.frame()==first,"unknown display command retains frame");
    b=present(1,1);ram.writeBytes(tls,b.data(),b.size());
    const auto page=tls&~uint64_t(4095);ram.setPermissions(page,4096,Memory::Read);
    cpu.writeRegister(0,display);p.dispatch(0x21,cpu,bus);
    check(cpu.readRegister(0)==Process::InvalidPointer && p.frame()==first && p.frameCount()==1,"failed IPC reply cannot publish new frame");
    ram.setPermissions(page,4096,Memory::Read|Memory::Write);
    check(send(present(1,1),display)==0 && p.frameCount()==2 && p.frame()->rgb[0]==9 && first->rgb[0]==1,"successful replacement keeps older snapshots stable");
    std::cout<<"Graphics client instructions="<<whole.instructions<<" hash="<<hash<<" failures="<<failures<<'\n';
    return failures?1:0;
}
