#include "machine.hpp"
#include "ipc.hpp"
#include <iostream>

static void put(Ipc::Buffer& b,size_t at,uint64_t v,size_t n=4)
{ for(size_t i=0;i<n;++i) b[at+i]=uint8_t(v>>(8*i)); }
static Ipc::Buffer copyRequest(uint64_t source,uint64_t destination,uint64_t size,uint64_t capacity)
{
    Ipc::Buffer b{};
    put(b,0,0x01100004); put(b,4,8); put(b,32,0x49434653); put(b,40,2);
    auto descriptor=[&](size_t at,uint64_t address,uint64_t length) {
        put(b,at,length); put(b,at+4,address);
        put(b,at+8,((address>>36)<<2) | (((length>>32)&15)<<24) | (((address>>32)&15)<<28));
    };
    descriptor(8,source,size); descriptor(20,destination,capacity); return b;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    int failures=0;
    auto check=[&](bool ok,const char* text) { if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } };
    Machine m(false,true);
    check(m.load(argv[1]),"load independent descriptor ELF");
    auto run=m.run(10000);
    check(run.stop==Machine::Stop::Exited && m.ram.read64(0x12000)==512 && m.ram.read32(0x12008)==1,"guest transfers and verifies 512 bytes");
    for(size_t i=0;i<512;++i) check(m.ram.read8(0x12400+i)==uint8_t(i),"all output bytes match independent pattern");
    Memory ram(Machine::RAM_SIZE); ProgramLoader::Image image; Process p; std::string error;
    check(ProgramLoader::loadImage(ram,argv[1],image,error) && p.initialize(ram,image,true,error),"buffer dispatcher fixture");
    Bus bus(ram); CPU cpu(bus); cpu.writeRegister(1,0x11000); p.dispatch(0x1F,cpu,bus);
    const auto session=cpu.readRegister(1), tls=p.mainThread().localRegion;
    ram.setPermissions(0x20000,0x22000,Memory::Read|Memory::Write);
    for(size_t i=0;i<Ipc::MaxTransfer;++i) ram.write8(0x20000+i,uint8_t(i*7+3));
    auto request=copyRequest(0x20000,0x31000,Ipc::MaxTransfer,Ipc::MaxTransfer);
    auto send=[&](const Ipc::Buffer& b) {
        ram.writeBytes(tls,b.data(),b.size()); cpu.writeRegister(0,session); p.dispatch(0x21,cpu,bus);
        return uint32_t(cpu.readRegister(0));
    };
    auto unchanged=[&](const Ipc::Buffer& b) {
        bool same=true; for(size_t i=0;i<b.size();++i) same &= ram.read8(tls+i)==b[i]; return same;
    };
    check(send(request)==0 && ram.read64(tls+32)==Ipc::MaxTransfer,"64 KiB transfer accepted across pages");
    for(size_t i=0;i<Ipc::MaxTransfer;++i) check(ram.read8(0x31000+i)==uint8_t(i*7+3),"large transfer content");
    auto version=ram.reservationVersion(0x31000);
    check(send(request)==0 && ram.reservationVersion(0x31000)!=version,"IPC output invalidates exclusive reservations");
    request=copyRequest(0x20000,0x20001,512,512);
    check(send(request)==0,"overlapping input and output allowed");
    for(size_t i=0;i<512;++i) check(ram.read8(0x20001+i)==uint8_t(i*7+3),"overlap uses input snapshot");
    request=copyRequest(0x20000,0x31000,512,511);
    const auto before=ram.read64(0x31000);
    check(send(request)==0 && ram.read32(tls+24)==Ipc::InvalidArguments && ram.read64(0x31000)==before,"small capacity is service error without data write");
    auto rejected=[&](Ipc::Buffer b,uint32_t expected,const char* message) {
        auto old=ram.read64(0x31000), rev=ram.reservationVersion(0x31000);
        check(send(b)==expected && unchanged(b) && ram.read64(0x31000)==old && ram.reservationVersion(0x31000)==rev,message);
    };
    rejected(copyRequest(0x20000,0x31000,Ipc::MaxTransfer+1,Ipc::MaxTransfer+1),Ipc::TransferLimit,"oversized transfer rejected before writes");
    rejected(copyRequest(0x20000,0x31000,1ULL<<32,1ULL<<32),Ipc::TransferLimit,"size high bits preserved");
    rejected(copyRequest(0x100020000ULL,0x31000,4,4),Ipc::InvalidBuffer,"address mid bits never truncated");
    rejected(copyRequest(0x1000020000ULL,0x31000,4,4),Ipc::InvalidBuffer,"address high bits never truncated");
    rejected(copyRequest(Machine::DEVICE_BASE,0x31000,4,4),Ipc::InvalidBuffer,"MMIO source rejected");
    rejected(copyRequest(0x20000,Machine::DEVICE_BASE,4,4),Ipc::InvalidBuffer,"MMIO destination rejected");
    rejected(copyRequest(0x20000,tls+16,4,4),Ipc::InvalidBuffer,"output cannot overwrite command buffer");
    rejected(copyRequest(0x20000,Machine::RAM_SIZE-2,4,4),Ipc::InvalidBuffer,"RAM boundary overflow rejected");
    rejected(copyRequest(0x11000,0x11000,4,4),Ipc::InvalidBuffer,"read-only output rejected");
    ram.setPermissions(0x21000,0x1000,Memory::None);
    rejected(copyRequest(0x20FF0,0x31000,32,32),Ipc::InvalidBuffer,"source crossing inaccessible page rejected");
    ram.setPermissions(0x21000,0x1000,Memory::Read|Memory::Write);
    ram.setPermissions(0x32000,0x1000,Memory::None);
    rejected(copyRequest(0x20000,0x31FF0,32,32),Ipc::InvalidBuffer,"output crossing inaccessible page rejected");
    ram.setPermissions(0x32000,0x1000,Memory::Read|Memory::Write);
    request=copyRequest(0x20000,0x31000,4,4); put(request,16,1);
    rejected(request,Ipc::UnsupportedFormat,"non-normal mapping mode rejected");
    request=copyRequest(0x20000,0x31000,4,4); put(request,4,1023);
    rejected(request,Ipc::InvalidMessage,"descriptor plus raw size must fit TLS");
    request=copyRequest(0x20000,0x31000,4,4); put(request,0,0x01200004);
    rejected(request,Ipc::UnsupportedFormat,"extra descriptor rejected");
    check(send(copyRequest(UINT64_MAX>>6,UINT64_MAX>>6,0,0))==0 && ram.read64(tls+32)==0,"zero-length descriptors do not dereference addresses");
    request=copyRequest(0x20000,0x31000,4,4); put(request,0,0x00100004);
    check(send(request)==0 && ram.read32(tls+24)==Ipc::InvalidArguments,"single-descriptor CMIF alignment parsed before service argument rejection");
    ram.write8(0x31008,0xA5);
    check(send(copyRequest(0x20000,0x31000,8,16))==0 && ram.read8(0x31008)==0xA5,"unused output capacity remains untouched");

    // Generic multi-range transaction: validate every output before committing either.
    const uint8_t bytes[]{1,2,3,4};
    const auto old=ram.read32(0x31000);
    const auto oldVersion=ram.reservationVersion(0x31000);
    const std::array<Memory::WriteRange,2> badWrites{{{0x31000,bytes,4},{0x11000,bytes,4}}};
    check(!ram.writeRanges(badWrites) && ram.read32(0x31000)==old && ram.reservationVersion(0x31000)==oldVersion,
        "second invalid output leaves first data and reservation unchanged");
    ram.clearError();
    const std::array<Memory::WriteRange,2> writes{{{0x31000,bytes,4},{0x31FFF,bytes,4}}};
    check(ram.writeRanges(writes) && ram.read32(0x31000)==0x04030201 && ram.read32(0x31FFF)==0x04030201,
        "two-range commit crosses pages successfully");
    std::cout<<"IPC buffer instructions="<<run.instructions<<" failures="<<failures<<'\n';
    return failures?1:0;
}
