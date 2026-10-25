#include "cpu.hpp"
#include "memory.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    try
    {
        std::ifstream file(argv[1],std::ios::binary);
        auto read=[&](char* destination,size_t count) {
            if(!file.read(destination,static_cast<std::streamsize>(count))) throw std::runtime_error("Truncated reference fixture");
        };
        auto integer=[&](unsigned count) {
            std::array<unsigned char,8> bytes{}; read(reinterpret_cast<char*>(bytes.data()),count);
            uint64_t value=0; for(unsigned i=0;i<count;++i) value|=uint64_t(bytes[i])<<(i*8);
            return value;
        };
        std::array<char,8> magic{}; read(magic.data(),magic.size());
        if(std::string(magic.data(),magic.size())!="A64REF01") throw std::runtime_error("Invalid reference format");
        const uint64_t count=integer(4);
        if(count==0 || count>100000) throw std::runtime_error("Invalid reference count");
        Memory ram(0x10000); CPU cpu(ram);
        std::array<uint8_t,512> initial{};
        for(size_t i=0;i<initial.size();++i) initial[i]=uint8_t(i*37+11);
        size_t failures=0;
        for(uint64_t test=0;test<count;++test)
        {
            const uint32_t instruction=uint32_t(integer(4)), options=uint32_t(integer(4));
            CPU::Context context;
            for(size_t i=0;i<31;++i) context.registers[i]=integer(8);
            context.sp=integer(8); context.pstate=integer(8)|0x1234; context.pc=0x1000;
            std::array<uint64_t,31> expected{};
            for(auto& value:expected) value=integer(8);
            const uint64_t expectedSP=integer(8), expectedFlags=integer(8), expectedPC=integer(8);
            std::array<uint8_t,512> expectedMemory{};
            if(options&1) read(reinterpret_cast<char*>(expectedMemory.data()),expectedMemory.size());
            ram.clearError();
            if(options&1) ram.writeBytes(0x4000,initial.data(),initial.size());
            ram.write32(0x1000,instruction); ram.write32(0x1004,0xD503201F);
            cpu.restoreContext(context);
            const auto result=cpu.stepDetailed();
            bool ok=(options&2) ? result.status==CPU::StepStatus::Error && cpu.hasError() : result.status==CPU::StepStatus::Executed;
            size_t mismatch=31;
            for(size_t i=0;i<31;++i) if(cpu.readRegister(i)!=expected[i]) { ok=false; mismatch=i; break; }
            ok=ok && cpu.getSP()==expectedSP && cpu.getPC()==expectedPC &&
                (cpu.getPSTATE()&0xF0000000)==expectedFlags && (cpu.getPSTATE()&~uint64_t(0xF0000000))==0x1234;
            if(options&1) for(size_t i=0;i<expectedMemory.size();++i)
                if(ram.read8(0x4000+i)!=expectedMemory[i]) { ok=false; break; }
            if(!ok)
            {
                if(failures<20)
                {
                    std::cerr<<"case="<<std::dec<<test<<" instruction=0x"<<std::hex<<instruction
                        <<" status="<<int(result.status)<<" pc="<<cpu.getPC()<<" expected_pc="<<expectedPC
                        <<" sp="<<cpu.getSP()<<" expected_sp="<<expectedSP
                        <<" flags="<<(cpu.getPSTATE()&0xF0000000)<<" expected_flags="<<expectedFlags;
                    if(mismatch<31) std::cerr<<" X"<<std::dec<<mismatch<<"="<<std::hex<<cpu.readRegister(mismatch)<<" expected="<<expected[mismatch];
                    if(cpu.hasError()) std::cerr<<" error="<<cpu.getLastError();
                    std::cerr<<'\n';
                }
                ++failures;
            }
        }
        if(file.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("Trailing reference data");
        std::cout<<std::dec<<"Independent ARM64 cases="<<count<<" failures="<<failures<<'\n';
        return failures==0 ? 0 : 1;
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 2; }
}
