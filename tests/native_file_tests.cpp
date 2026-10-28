#include "machine.hpp"
#include "ipc.hpp"
#include <fstream>
#include <map>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    std::map<std::string,uint64_t> symbols;std::ifstream in(argv[2]);std::string name;uint64_t value;
    while(in>>name>>std::hex>>value)symbols[name]=value;
    for(auto key:{"main","fsdevGetDeviceFileSystem","fsFsOpenFile","fsFileRead","fsFileGetSize","fsFileClose"})if(!symbols.contains(key))return 2;
    Machine m(false,true,true);
    if(!m.mountReadOnlyRoot(std::filesystem::path(argv[1]).parent_path().parent_path()/"hle_files/root") || !m.load(argv[1]))return 2;
    for(unsigned i=0;m.cpu.getPC()!=symbols.at("main");++i)if(i==500000 || m.run(1).stop!=Machine::Stop::Budget)return 2;
    const auto context=m.cpu.captureContext();const auto ipc=m.process().mainThread().localRegion;
    constexpr uint64_t ret=Process::WorkerStackBase,path=ret+0x1000,file=ret+0x2000,output=ret+0x3000,countAt=ret+0x4000;
    Ipc::Buffer openPacket{},readPacket{};uint64_t svcPC=0;uint32_t session=0;
    auto put=[](Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4){for(size_t i=0;i<n;++i)b[at+i]=uint8_t(v>>(8*i));};
    auto call=[&](const char* function,std::array<uint64_t,6> args={}){
        m.cpu.restoreContext(context);m.cpu.setPC(symbols.at(function));for(unsigned r=0;r<6;++r)m.cpu.writeRegister(r,args[r]);m.cpu.writeRegister(30,ret);
        for(unsigned i=0;i<500000;++i){
            if(m.cpu.getPC()==ret)return m.cpu.readRegister(0);
            const auto instruction=m.ram.fetch32(m.cpu.getPC());
            if((instruction&0xFFE0001F)==0xD4000001 && ((instruction>>5)&0xFFFF)==0x21){
                Ipc::Buffer b{};for(size_t j=0;j<b.size();++j)b[j]=m.ram.read8(ipc+j);
                Ipc::DomainRequest d;if(!Ipc::parseDomain(b,d,true)){
                    if(d.command==8 && d.sendStatic)openPacket=b;
                    if(d.command==0 && d.receiveBuffer)readPacket=b;
                    svcPC=m.cpu.getPC();session=uint32_t(m.cpu.readRegister(0));
                }
            }
            if(m.run(1).stop!=Machine::Stop::Budget)throw std::runtime_error(m.error());
        }
        throw std::runtime_error("Native file call budget exceeded");
    };
    auto text=[&](const std::string& s){for(unsigned i=0;i<0x301;++i)m.ram.write8(path+i,0);m.ram.writeBytes(path,reinterpret_cast<const uint8_t*>(s.c_str()),s.size()+1);};
    auto replay=[&](const Ipc::Buffer& b){m.ram.writeBytes(ipc,b.data(),b.size());m.cpu.restoreContext(context);m.cpu.setPC(svcPC);m.cpu.writeRegister(0,session);const auto step=m.run(1);check(step.stop==Machine::Stop::Budget,"replayed SDK packet returns without process fault");return m.cpu.readRegister(0);};
    try{
        text("sdmc");const auto fs=call("fsdevGetDeviceFileSystem",{path});check(fs!=0,"original fsdev provides mounted filesystem");
        text("/sample.txt");check(call("fsFsOpenFile",{fs,path,1,file})==0,"original libnx opens native read-only file");
        const auto firstId=m.ram.read32(file+8);check(firstId==3,"native file has distinct domain object identity");
        check(call("fsFileGetSize",{file,countAt})==0 && m.ram.read64(countAt)==30,"original libnx decodes native file size");
        for(unsigned i=0;i<128;++i)m.ram.write8(output+i,0xA5);
        check(call("fsFileRead",{file,0,output,64,0,countAt})==0 && m.ram.read64(countAt)==30,"native read truncates to actual file length");
        const std::string expected="Switch2Emu read-only fixture.\n";
        check(expected.size()==30,"fixture length matches reference");
        for(size_t i=0;i<expected.size();++i)check(m.ram.read8(output+i)==uint8_t(expected[i]),"original read returns exact snapshot bytes");
        for(unsigned i=30;i<128;++i)check(m.ram.read8(output+i)==0xA5,"read preserves unused output capacity");
        check(call("fsFileRead",{file,21,output,32,0,countAt})==0 && m.ram.read64(countAt)==9,"partial read returns remaining bytes");
        for(unsigned i=0;i<9;++i)check(m.ram.read8(output+i)==uint8_t(expected[21+i]),"partial bytes match offset");
        std::array<uint8_t,128> beforeEof{};for(size_t i=0;i<beforeEof.size();++i)beforeEof[i]=m.ram.read8(output+i);
        check(call("fsFileRead",{file,30,output,32,0,countAt})==0 && m.ram.read64(countAt)==0,"EOF read returns zero bytes");
        for(size_t i=0;i<beforeEof.size();++i)check(beforeEof[i]==m.ram.read8(output+i),"EOF preserves all output bytes");
        check(call("fsFileRead",{file,UINT64_MAX,output,32,0,countAt})==Ipc::InvalidArguments,"negative native offset is rejected");
        check(call("fsFileRead",{file,0,output,32,1,countAt})==Ipc::InvalidArguments,"unsupported read options are explicit");
        check(call("fsFileRead",{file,0,UINT64_MAX,0,0,countAt})==0 && m.ram.read64(countAt)==0,"zero-length native read does not dereference output");
        text("/../sample.txt");check(call("fsFsOpenFile",{fs,path,1,file+32})==Ipc::InvalidPath,"native traversal is rejected");
        text("sample.txt");check(call("fsFsOpenFile",{fs,path,1,file+32})==Ipc::InvalidPath,"native path must be root-absolute");
        text("/missing.txt");check(call("fsFsOpenFile",{fs,path,1,file+32})==Ipc::FileNotFound,"missing native file is explicit");
        text("/sample.txt");check(call("fsFsOpenFile",{fs,path,2,file+32})==Ipc::InvalidArguments,"write mode cannot open read-only snapshot");
        // Capture a valid packet again, then verify transport errors leave both outputs intact.
        call("fsFileRead",{file,0,output,64,0,countAt});
        auto bad=readPacket;put(bad,12,ipc);check(replay(bad)==Ipc::InvalidBuffer,"native output cannot overlap IPC reply");
        bad=readPacket;put(bad,16,5);check(replay(bad)==Ipc::InvalidBuffer,"high native buffer address is not truncated");
        bad=readPacket;put(bad,8,Ipc::MaxTransfer+1);check(replay(bad)==Ipc::TransferLimit,"native output capacity obeys transfer limit");
        bad=readPacket;put(bad,16,2);check(replay(bad)==Ipc::UnsupportedFormat,"reserved native mapping mode is rejected");
        m.ram.setPermissions(output,Memory::PAGE_SIZE,Memory::Read);
        check(replay(readPacket)==Ipc::InvalidBuffer,"read-only destination rejects complete native read");m.ram.setPermissions(output,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
        for(unsigned i=0;i<64;++i)m.ram.write8(output+i,0xCC);
        m.ram.writeBytes(ipc,readPacket.data(),readPacket.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);
        m.cpu.restoreContext(context);m.cpu.setPC(svcPC);m.cpu.writeRegister(0,session);m.run(1);
        check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected read reply prevents grouped publication");
        for(unsigned i=0;i<64;++i)check(m.ram.read8(output+i)==0xCC,"failed reply leaves read output intact");
        for(size_t i=0;i<readPacket.size();++i)check(m.ram.read8(ipc+i)==readPacket[i],"failed reply preserves request bytes");
        m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
        text("/sample.txt");call("fsFsOpenFile",{fs,path,1,file+32});const auto secondId=m.ram.read32(file+40);
        // A protected reply cannot publish a new file object or consume an ID.
        m.ram.writeBytes(ipc,openPacket.data(),openPacket.size());m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read);
        m.cpu.restoreContext(context);m.cpu.setPC(svcPC);m.cpu.writeRegister(0,session);m.run(1);
        check(m.cpu.readRegister(0)==Process::InvalidPointer,"protected open reply rejects publication");m.ram.setPermissions(ipc,Memory::PAGE_SIZE,Memory::Read|Memory::Write);
        check(call("fsFsOpenFile",{fs,path,1,file+64})==0 && m.ram.read32(file+72)==secondId+1,"failed publication consumes no file object ID");
        call("fsFileClose",{file});check(call("fsFileGetSize",{file,countAt})!=0,"closed native file cannot be queried");
        call("fsFileClose",{file+32});call("fsFileClose",{file+64});
        m.cpu.restoreContext(context);check(m.run(500000).stop==Machine::Stop::Exited,"file calls preserve original main and cleanup");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"Native file failures="<<failures<<'\n';return failures?1:0;
}
