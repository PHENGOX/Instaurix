#include "memory.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <utility>
namespace { long failAfter=-1; }
// Exercise allocation-failure boundaries without adding production test hooks.
void* operator new(std::size_t bytes){if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;if(void* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t bytes){return ::operator new(bytes);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
int main(){
    unsigned failures=0;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Memory ram(0x4003);check(!ram.reservationVersion(0x4003) && !ram.reservationVersion(UINT64_MAX),"invalid reservation addresses remain zero");
    failAfter=0;const auto cold=ram.read64(0x1000),version=ram.reservationVersion(0x1000);failAfter=-1;check(!cold && !version && !ram.hasError(),"cold reads and version lookups allocate no tracking rows");
    check(ram.setPermissions(0x1000,4096,Memory::All),"cold permission change succeeds");const auto baseline=ram.reservationVersion(0x1000);check(baseline && ram.reservationVersion(0x1010)==baseline,"cold page records invalidation without a granule array");
    ram.write8(0x100F,0);check(ram.reservationVersion(0x1000)!=baseline && ram.reservationVersion(0x1010)==baseline,"first row allocation preserves untouched granules at the current page baseline");
    const auto first=ram.reservationVersion(0x1000);ram.write64(0x1010,0x1122334455667788ULL);check(ram.reservationVersion(0x1000)==first && ram.reservationVersion(0x1010)!=baseline,"different granules in the same page remain independent");
    const auto written=ram.reservationVersion(0x1010);ram.write64(0x1010,0);ram.write64(0x1010,0x1122334455667788ULL);check(ram.reservationVersion(0x1010)!=written,"same-value and ABA writes invalidate old reservations");
    ram.write16(0x1FFF,0xABCD);check(ram.read8(0x1FFF)==0xCD && ram.read8(0x2000)==0xAB && ram.reservationVersion(0x1FFF)==ram.reservationVersion(0x2000),"cross-page scalar writes preallocate and invalidate both touched granules together");
    const auto outside=ram.reservationVersion(0x2000),old=ram.reservationVersion(0x1000);failAfter=0;const bool permissions=ram.setPermissions(0x1000,4096,Memory::All);failAfter=-1;check(permissions && ram.reservationVersion(0x1000)!=old && ram.reservationVersion(0x1000)==ram.reservationVersion(0x1FF0) && ram.reservationVersion(0x2000)==outside,"page invalidation allocates nothing and leaves other pages untouched");
    const auto replacement=ram.reservationVersion(0x1000);ram.write8(0x1010,1);check(ram.reservationVersion(0x1000)==replacement && ram.reservationVersion(0x1010)!=replacement,"recreated tracking rows preserve the replacement page epoch");
    const auto previous=ram.reservationVersion(0x1000);failAfter=0;const bool discarded=ram.discardPages(0x1000,4096);failAfter=-1;check(discarded && ram.reservationVersion(0x1000)!=previous,"discard invalidates without allocating replacement metadata");ram.setPermissions(0x1000,4096,Memory::All);check(!ram.read64(0x1000),"discarded backing reads as zero after remapping");
    const auto beforeReset=ram.reservationVersion(0x3000);failAfter=0;ram.reset();failAfter=-1;check(ram.reservationVersion(0x3000)!=beforeReset && !ram.read64(0x1010),"reset invalidates even untouched pages without allocating rows");const auto resetVersion=ram.reservationVersion(0x1000);ram.write8(0x1010,0);check(ram.reservationVersion(0x1000)==resetVersion,"first write after reset preserves cold-granule reservations");
    ram.write8(0x4002,0x7B);check(ram.read8(0x4002)==0x7B && ram.setPermissions(0x4000,3,Memory::Read),"partial final page uses the same sparse tracking rules");const auto lastVersion=ram.reservationVersion(0x4002);ram.write8(0x4002,0);check(ram.hasError() && ram.reservationVersion(0x4002)==lastVersion,"denied final-page write does not invalidate a reservation");ram.clearError();
    Memory moved(std::move(ram));check(moved.read8(0x4002)==0x7B && moved.reservationVersion(0x4002)==lastVersion,"memory move transfers tracking baselines and rows with the data");
    unsigned allocationFailures=0;bool completed=false;
    for(long boundary=0;boundary<8;++boundary){
        Memory transaction(0x4000);transaction.write64(0x1000,0x8877665544332211ULL);const auto a=transaction.reservationVersion(0x1000),b=transaction.reservationVersion(0x2000);const uint8_t bytes[]{1,2,3,4};const std::array<Memory::WriteRange,2> ranges{{{0x1000,bytes,4},{0x2000,bytes,4}}};
        bool success=false;failAfter=boundary;try{success=transaction.writeRanges(ranges);}catch(...){failAfter=-1;check(false,"writeRanges must report allocation failure instead of throwing");}failAfter=-1;
        if(success){completed=true;check(transaction.read32(0x1000)==0x04030201 && transaction.read32(0x2000)==0x04030201,"complete multi-range write commits after all allocation points");break;}
        ++allocationFailures;check(transaction.hasError() && transaction.read64(0x1000)==0x8877665544332211ULL && !transaction.read64(0x2000) && transaction.reservationVersion(0x1000)==a && transaction.reservationVersion(0x2000)==b,"failure at every allocation point preserves data and reservation versions in every range");
    }
    check(completed && allocationFailures>=2,"both backing and tracking allocation failures are exercised");
    for(long boundary=0;boundary<2;++boundary){Memory scalar(0x2000);const auto v=scalar.reservationVersion(0x1000);failAfter=boundary;try{scalar.write64(0x1000,UINT64_MAX);}catch(...){failAfter=-1;check(false,"scalar writes must catch allocation failure");}failAfter=-1;check(scalar.hasError() && !scalar.read64(0x1000) && scalar.reservationVersion(0x1000)==v,"scalar allocation failure has no partial bytes or invalidation");}
    std::cout<<"Sparse reservation tracking failures="<<failures<<'\n';return failures?1:0;
}
