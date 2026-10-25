#include "machine.hpp"
#include "ipc.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
static void put(Ipc::Buffer& b,size_t at,uint64_t value,size_t n=4)
{ for(size_t i=0;i<n;++i) b[at+i]=uint8_t(value>>(8*i)); }
static Ipc::Buffer request(uint32_t command)
{ Ipc::Buffer b{}; put(b,0,4); put(b,4,8); put(b,16,0x49434653); put(b,24,command); return b; }
int main(int argc,char** argv)
{
    if(argc!=4) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    const auto base=fs::absolute(argv[3]).lexically_normal();
    const auto temp=base/("files-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(temp.parent_path()!=base || !fs::create_directory(temp)) return 2;
    const auto root=temp/"root"; fs::create_directory(root);
    fs::copy_file(fs::path(argv[2])/"sample.txt",root/"sample.txt");
    std::string error;
    auto snapshot=ReadOnlyFiles::load(root,error);
    check(snapshot && snapshot->find("sample.txt") && snapshot->find("sample.txt")->size()==30,"mount flat read-only snapshot");
    Machine m(false,true), sliced(false,true), disabled(false,true);
    check(m.mountReadOnlyRoot(root) && sliced.mountReadOnlyRoot(root) && m.load(argv[1]) && sliced.load(argv[1]),"mount before compiled client load");
    fs::remove(root/"sample.txt");
    check(!m.mountReadOnlyRoot(temp/"missing"),"failed mount keeps previous configuration");
    auto whole=m.run(50000);
    uint32_t hash=2166136261U;
    const std::string text="Switch2Emu read-only fixture.\n";
    for(unsigned char c:text) hash=(hash^c)*16777619U;
    check(whole.stop==Machine::Stop::Exited && m.ram.read64(0x12000)==1 && m.ram.read64(0x12008)==30 &&
        m.ram.read64(0x12010)==30 && m.ram.read64(0x12018)==hash && m.ram.read64(0x12020)==9 && m.ram.read64(0x12028)==0,
        "C reads size content partial and EOF after host file deletion");
    uint64_t count=0;
    for(unsigned i=0;i<10000;++i) { auto r=sliced.run(7); count+=r.instructions; if(r.stop==Machine::Stop::Exited) break; }
    check(count==whole.instructions && sliced.ram.read64(0x12000)==1,"file client deterministic under sliced execution");
    check(m.load(argv[1]) && m.run(50000).stop==Machine::Stop::Exited && m.ram.read64(0x12000)==1,"failed mount rollback survives reload");
    check(disabled.load(argv[1]) && disabled.run(50000).stop==Machine::Stop::Exited && disabled.ram.read64(0x12000)==2,"file service disabled without explicit mount");
    Machine raw;
    check(!raw.mountReadOnlyRoot(argv[2]),"mount requires reference HLE mode");

    Memory ram(Machine::RAM_SIZE); Process p; ProgramLoader::Image image;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"filesystem dispatcher fixture");
    Bus bus(ram); CPU cpu(bus); const auto tls=p.mainThread().localRegion;
    const uint8_t port[]{'s','m',':',0}; ram.writeBytes(0x12800,port,sizeof port);
    cpu.writeRegister(1,0x12800); p.dispatch(0x1F,cpu,bus); const auto sm=uint32_t(cpu.readRegister(1));
    auto send=[&](const Ipc::Buffer& b,uint32_t handle) { ram.writeBytes(tls,b.data(),b.size()); cpu.writeRegister(0,handle); p.dispatch(0x21,cpu,bus); return uint32_t(cpu.readRegister(0)); };
    auto init=request(0); put(init,4,0x8000000A); put(init,8,1); put(init,16,0); put(init,32,0x49434653);
    check(send(init,sm)==0 && ram.read32(tls+24)==0,"initialize manager");
    auto get=request(1); put(get,4,10); const char* service="emu:fs";
    for(size_t i=0;i<6;++i) get[32+i]=uint8_t(service[i]);
    check(send(get,sm)==0 && ram.read32(tls+24)==Ipc::FilesDisabled,"disabled service is explicit SFCO error");
    p.setReadOnlyFiles(snapshot);
    check(send(get,sm)==0 && ram.read32(tls+24)==0,"lookup mounted filesystem service");
    const auto files=ram.read32(tls+12);
    auto open=[&](const std::string& name) {
        ram.writeBytes(0x12840,reinterpret_cast<const uint8_t*>(name.c_str()),name.size()+1);
        Ipc::Buffer b{}; put(b,0,0x00100004); put(b,4,8); put(b,8,name.size()+1); put(b,12,0x12840); put(b,32,0x49434653);
        return send(b,files);
    };
    for(const auto& name : {"../sample.txt","/sample.txt","C:sample.txt","a\\b",".",".."})
        check(open(name)==0 && ram.read32(tls+24)==Ipc::InvalidPath,"unsafe guest name refused without host access");
    check(open("missing.txt")==0 && ram.read32(tls+24)==Ipc::FileNotFound,"missing file error");
    check(open("sample.txt")==0 && ram.read32(tls+24)==0,"open snapshot file with moved handle");
    const auto file=ram.read32(tls+12);
    cpu.writeRegister(0,files); p.dispatch(0x16,cpu,bus);
    check(send(request(1),file)==0 && ram.read64(tls+32)==30,"file stays valid after filesystem session closes");
    auto read=[&](uint64_t offset,uint64_t length,uint32_t capacity=64,uint32_t destination=0x12900) {
        Ipc::Buffer b{}; put(b,0,0x01000004); put(b,4,12); put(b,8,capacity); put(b,12,destination);
        put(b,32,0x49434653); put(b,48,offset,8); put(b,56,length,8); return send(b,file);
    };
    ram.write8(0x12900,0xA5);
    check(read(UINT64_MAX,64)==0 && ram.read64(tls+32)==0 && ram.read8(0x12900)==0xA5,"large offset EOF without overflow or output mutation");
    check(read(0,65)==0 && ram.read32(tls+24)==Ipc::InvalidArguments && ram.read8(0x12900)==0xA5,"read length cannot exceed destination capacity");
    check(read(0,4,64,0x11000)==Ipc::InvalidBuffer,"read-only destination rejected");
    check(read(0,0)==0 && ram.read64(tls+32)==0,"zero-length read");
    check(read(27,64)==0 && ram.read64(tls+32)==3 && ram.read8(0x12900)=='e' && ram.read8(0x12902)=='\n',"short read at EOF");
    check(send(request(2),file)==0 && ram.read32(tls+24)==Ipc::UnknownCommand,"write operation never accepted");
    check(snapshot->find("sample.txt")->size()==30,"snapshot content not changed by guest operations");

    const auto invalid=temp/"invalid"; fs::create_directory(invalid); fs::create_directory(invalid/"subdir");
    check(!ReadOnlyFiles::load(invalid,error),"subdirectories rejected rather than traversed");
    fs::remove(invalid/"subdir");
    { std::ofstream out(invalid/"huge.bin",std::ios::binary); out.seekp(ReadOnlyFiles::MaxFileSize); out.put('x'); }
    check(!ReadOnlyFiles::load(invalid,error),"oversized file rejected");
    fs::remove(invalid/"huge.bin");
    for(size_t i=0;i<ReadOnlyFiles::MaxFiles+1;++i) { std::ofstream out(invalid/(std::to_string(i)+".bin")); }
    check(!ReadOnlyFiles::load(invalid,error),"file count bound enforced");
    const auto total=temp/"total"; fs::create_directory(total);
    for(unsigned i=0;i<3;++i) {
        std::ofstream out(total/(std::to_string(i)+".bin"),std::ios::binary);
        if(i<2) out.seekp(ReadOnlyFiles::MaxFileSize-1);
        out.put('x');
    }
    check(!ReadOnlyFiles::load(total,error),"total snapshot memory bound enforced");
    const auto links=temp/"links"; fs::create_directory(links);
    std::error_code ec; fs::create_symlink(fs::absolute(fs::path(argv[2])/"sample.txt"),links/"link.txt",ec);
    if(!ec) check(!ReadOnlyFiles::load(links,error),"symbolic link cannot import external content");
    else std::cout<<"Symlink creation unavailable: "<<ec.message()<<'\n';
    check(!ReadOnlyFiles::validName("file:stream") && !ReadOnlyFiles::validName("a/b") && !ReadOnlyFiles::validName(std::string(65,'a')),"strict flat names exclude streams paths and long names");
    if(temp.parent_path()==base) fs::remove_all(temp);
    std::cout<<"File client instructions="<<whole.instructions<<" hash="<<hash<<" failures="<<failures<<'\n';
    return failures?1:0;
}
