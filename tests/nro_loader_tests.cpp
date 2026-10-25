#include "machine.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
static void put(std::vector<uint8_t>& b,size_t at,uint32_t value)
{ for(size_t i=0;i<4;++i) b[at+i]=uint8_t(value>>(8*i)); }
static uint32_t read(const std::vector<uint8_t>& b,size_t at)
{ uint32_t v=0;for(size_t i=0;i<4;++i) v|=uint32_t(b[at+i])<<(8*i);return v; }
int main(int argc,char** argv)
{
    if(argc!=4) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures;std::cerr<<"FAIL: "<<text<<'\n'; } };
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
    if(bytes.size()<128) return 2;
    Memory ram(Machine::RAM_SIZE);ProgramLoader::Image image;std::string error;
    check(ProgramLoader::loadNro(ram,bytes,image,error) && image.format==ProgramLoader::Format::Nro &&
        image.entry==0x10000 && !image.requiresNroRuntime,"load NRO0 reference without MOD0");
    check(ram.canAccess(0x10000,4096,Memory::Read|Memory::Execute) && !ram.canAccess(0x10000,1,Memory::Write),"text RX");
    check(ram.canAccess(0x11000,4096,Memory::Read) && !ram.canAccess(0x11000,1,Memory::Write|Memory::Execute),"rodata R");
    const auto data=uint64_t(0x10000)+read(bytes,0x30);
    const auto dataSize=read(bytes,0x34), bss=read(bytes,0x38);
    check(bss>=8192 && ram.canAccess(data,dataSize+bss,Memory::Read|Memory::Write) && !ram.canAccess(data,1,Memory::Execute),"data and extended BSS RW NX");
    for(uint64_t at=data+dataSize;at<data+dataSize+bss;++at) check(ram.read8(at)==0,"BSS zero initialized");
    check(!ram.canAccess(Machine::RAM_SIZE-0x11000,4096,Memory::Read) &&
        ram.canAccess(Machine::RAM_SIZE-0x10000,0x10000,Memory::Read|Memory::Write),"stack and guard preserved");
    Machine nro(false,true), elf(false,true);
    check(nro.load(argv[1]) && elf.load(argv[2]),"load NRO and ELF versions");
    auto result=nro.run(50000), reference=elf.run(50000);
    check(result.stop==Machine::Stop::Exited && reference.stop==Machine::Stop::Exited && result.instructions==reference.instructions,
        "NRO and corresponding ELF execute identical instruction path");
    for(size_t i=0;i<7;++i) check(nro.ram.read64(0x12000+i*8)==elf.ram.read64(0x12000+i*8),"compiled C report identical across file formats");
    check(nro.ram.read64(0x12000)==1 && nro.ram.read64(0x12008)==55 && nro.ram.read64(0x12010)==512 &&
        nro.ram.read64(0x12018)==3505779141ULL,"NRO C client exercises sm sessions IPC copying and hash");
    auto rejected=[&](const std::vector<uint8_t>& bad,const char* label) {
        const auto before=ram.read64(data);const auto entry=image.entry;const auto format=image.format;
        check(!ProgramLoader::loadNro(ram,bad,image,error) && !error.empty() && ram.read64(data)==before &&
            image.entry==entry && image.format==format,label);
    };
    for(auto [at,value]:std::vector<std::pair<size_t,uint32_t>>{
        {0x10,0},{0x14,1},{0x18,0},{0x1C,1},{0x1C,2},{0x3C,0x80},{0x20,4096},{0x24,4095},
        {0x28,0},{0x28,0xFFFFF000},{0x2C,0xFFFFF000},{0x30,0},{0x34,0xFFFFF000},
        {0x38,0xFFFFFFFF},{0x60,1},{4,0xFFFFFFFF},{4,0x80}}) {
        auto bad=bytes;put(bad,at,value);rejected(bad,"invalid header fields rejected without replacing image");
    }
    for(size_t length:{size_t(0),size_t(16),size_t(127),bytes.size()-1}) {
        auto bad=bytes;bad.resize(length);rejected(bad,"truncated NRO rejected");
    }
    auto extra=bytes;extra.push_back(0);rejected(extra,"appended assets not silently ignored");
    const auto path=std::filesystem::path(argv[3])/"nro-loader-test.NRO";
    auto save=[&](const auto& file,const std::vector<uint8_t>& data) {
        std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(data.data()),data.size());return bool(out);
    };
    auto module=bytes;put(module,4,0x100);put(module,0x100,0x30444F4D);
    check(ProgramLoader::loadNro(ram,module,image,error) && image.requiresNroRuntime,"valid MOD0 recognized for format inspection");
    check(save(path,module),"write MOD0 test sample");
    const auto oldPc=nro.cpu.getPC(), oldStatus=nro.ram.read64(0x12000);
    check(!nro.load(path.string().c_str()) && nro.error().find("MOD0")!=std::string::npos && nro.cpu.getPC()==oldPc &&
        nro.ram.read64(0x12000)==oldStatus,"runtime blocker diagnosed before changing current session");
    check(save(path,bytes) && nro.load(path.string().c_str()),"uppercase NRO extension accepted");
    const auto renamed=std::filesystem::path(argv[3])/"nro-magic-test.bin";
    check(save(renamed,bytes) && nro.load(renamed.string().c_str()),"NRO detected by magic rather than raw fallback");
    const auto nso=std::filesystem::path(argv[3])/"unsupported-format.nso";
    check(save(nso,std::vector<uint8_t>{0x1F,0x20,0x03,0xD5}) && !nro.load(nso.string().c_str()) &&
        nro.error().find("NSO")!=std::string::npos,"unsupported NSO never executed as raw instructions");
    const auto container=std::filesystem::path(argv[3])/"unsupported-container.nca";
    check(save(container,std::vector<uint8_t>{0x1F,0x20,0x03,0xD5}) && !nro.load(container.string().c_str()),"content container never executed as raw instructions");
    std::filesystem::remove(path);std::filesystem::remove(renamed);std::filesystem::remove(nso);
    std::filesystem::remove(container);
    std::cout<<"NRO client instructions="<<result.instructions<<" failures="<<failures<<'\n';
    return failures?1:0;
}
