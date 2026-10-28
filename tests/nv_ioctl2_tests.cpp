#include "ipc_nv.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include <iostream>
static void put(Ipc::Buffer& b,size_t at,uint32_t v){for(size_t i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
static Ipc::Buffer packet(){Ipc::Buffer b{};put(b,0,0x01220004);put(b,4,0xC0B);put(b,16,1);put(b,24,24);put(b,28,0x4000);put(b,36,8);put(b,40,0x8000);put(b,48,24);put(b,52,0x4000);put(b,64,0x49434653);put(b,72,11);put(b,80,3);put(b,84,0xC018481B);return b;}
int main(){
    Memory memory(0x200000);Bus bus(memory);unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    const auto good=packet();Ipc::NvIoctl output;
    check(!Ipc::parseNvIoctl(good,bus,0x1000,output) && output.command==11 && output.fd==3 && output.request==0xC018481B && output.source==0x4000 && output.destination==0x4000 && output.size==24 && output.extraSource==0x8000 && output.extraSize==8,"ioctl2 resolves three distinct buffer roles");
    auto reject=[&](const Ipc::Buffer& b,uint32_t error){Ipc::NvIoctl p{9,10,11,12,13,14,15,16};check(Ipc::parseNvIoctl(b,bus,0x1000,p)==error,"invalid transport returns its explicit error");check(p.fd==9 && p.request==10 && p.source==11 && p.destination==12 && p.size==13 && p.command==14 && p.extraSource==15 && p.extraSize==16,"failed parse never publishes a partial plan");};
    for(const auto at:{0U,4U,8U,16U,20U,68U,72U,76U,100U,104U}){auto b=good;put(b,at,at==16?0:1);reject(b,Ipc::UnsupportedFormat);}
    {auto b=good;put(b,64,0);reject(b,Ipc::InvalidMessage);}
    for(const auto at:{32U,44U,56U}){auto b=good;put(b,at,1);reject(b,Ipc::UnsupportedFormat);}
    for(const auto at:{24U,48U}){auto b=good;put(b,at,23);reject(b,Ipc::InvalidArguments);}
    {auto b=good;put(b,84,0x8018481B);reject(b,Ipc::UnsupportedFormat);put(b,84,0xC000481B);reject(b,Ipc::UnsupportedFormat);}
    for(const auto at:{24U,36U,48U}){auto b=good;put(b,at,uint32_t(Ipc::MaxTransfer+1));reject(b,Ipc::TransferLimit);}
    {auto b=good;put(b,44,1U<<24);reject(b,Ipc::TransferLimit);}
    for(const auto at:{28U,40U,52U}){auto b=good;put(b,at,0x10000000);reject(b,Ipc::InvalidBuffer);put(b,at,0x1000);reject(b,Ipc::InvalidBuffer);}
    {auto b=good;put(b,44,0xF0000000);reject(b,Ipc::InvalidBuffer);}
    {auto b=good;put(b,52,0x4008);reject(b,Ipc::InvalidBuffer);}
    {auto b=good;put(b,40,0x4000);reject(b,Ipc::InvalidBuffer);put(b,40,0x4014);reject(b,Ipc::InvalidBuffer);put(b,40,0x3FFC);reject(b,Ipc::InvalidBuffer);}
    {auto b=good;put(b,40,uint32_t(memory.size()-8));check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"extra input may end exactly at RAM end");put(b,40,uint32_t(memory.size()-4));reject(b,Ipc::InvalidBuffer);}
    {auto b=good;put(b,36,uint32_t(Ipc::MaxTransfer));check(!Ipc::parseNvIoctl(b,bus,0x1000,output) && output.extraSize==Ipc::MaxTransfer,"extra input has its own bounded transfer length");}
    {auto b=good;put(b,36,9);check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"generic transport does not pretend to validate method-specific entry counts");}
    {auto b=good;put(b,36,0);put(b,40,0);check(!Ipc::parseNvIoctl(b,bus,0x1000,output) && !output.extraSize,"empty extra input is accepted without memory access");put(b,40,UINT32_MAX);check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"empty extra input does not dereference an ignored pointer");}
    memory.setPermissions(0x8000,4096,Memory::Read);check(!Ipc::parseNvIoctl(good,bus,0x1000,output),"extra input needs read permission only");memory.setPermissions(0x8000,4096,Memory::None);reject(good,Ipc::InvalidBuffer);memory.setPermissions(0x8000,4096,Memory::All);
    {auto b=good;put(b,52,0x5000);memory.setPermissions(0x4000,4096,Memory::Read);check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"disjoint output allows read-only parameter input");memory.setPermissions(0x5000,4096,Memory::Read);reject(b,Ipc::InvalidBuffer);memory.setPermissions(0x4000,8192,Memory::All);put(b,40,0x4010);check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"two input ranges may overlap when the writable receive range is separate");}
    {auto b=good;put(b,36,8192);memory.setPermissions(0x9000,4096,Memory::None);reject(b,Ipc::InvalidBuffer);memory.setPermissions(0x9000,4096,Memory::All);check(!Ipc::parseNvIoctl(b,bus,0x1000,output),"all pages of extra input are checked");}
    {auto b=good;put(b,24,4);put(b,84,0x4004480D);for(size_t i=48;i<60;i+=4)put(b,i,0);check(!Ipc::parseNvIoctl(b,bus,0x1000,output) && output.command==11 && !output.destination && output.size==4,"ioctl2 write-only parameters require a fully null receive descriptor");put(b,52,0x4000);reject(b,Ipc::InvalidArguments);}
    {Ipc::Buffer b{};put(b,0,0x01110004);put(b,4,0xC0B);put(b,16,4);put(b,20,0x4000);put(b,48,0x49434653);put(b,56,1);put(b,64,3);put(b,68,0x4004480D);check(!Ipc::parseNvIoctl(b,bus,0x1000,output) && output.command==1 && !output.extraSize && !output.extraSource,"existing ioctl command 1 keeps its separate layout and semantics");put(b,56,11);reject(b,Ipc::UnsupportedFormat);}
    check(!bus.hasError(),"transport preflight does not perform failing reads or writes");
    std::cout<<"NV ioctl2 parser failures="<<failures<<'\n';return failures?1:0;
}
