#include "gpu_va.hpp"
#include <iostream>
int main(){
    using S=Gpu::AddressSpace;using R=S::Result;constexpr auto page=S::PageSize;unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    S space;Gpu::VirtualAddress a{},b{},fixed{};Gpu::AddressSpace::Translation result{99,77};
    check(space.map(7,2*page,0,2*page,{},a)==R::Success && a.value==S::Base && space.references(7),"automatic mapping reserves a range and retains its object identity");
    check(space.translate(Gpu::VirtualAddress{a.value+page+17},31,result) && result.object==7 && result.offset==page+17,"translation preserves byte offsets across pages");
    result={99,77};check(!space.translate(Gpu::VirtualAddress{a.value+2*page-1},2,result) && result.object==99 && result.offset==77,"translation cannot cross the mapped extent or modify output on failure");
    check(!space.translate(a,0,result) && !space.translate(Gpu::VirtualAddress{UINT64_MAX},1,result),"zero/overflowing translations fail");
    check(space.release(a,2*page)==R::Occupied,"mapped reservation cannot be freed");
    check(space.map(7,2*page,page,page,{},b)==R::Success,"one object can have independent alias mappings");
    check(space.unmap(a)==R::Success && space.references(7) && space.unmap(a)==R::InvalidAddress,"unmapping one alias preserves the other reference");
    check(space.unmap(b)==R::Success && !space.references(7) && !space.count(),"automatic unmap reclaims its reservation and final object reference");
    check(space.reserve(page,page,{},fixed)==R::Success && space.map(11,2*page,page,page,fixed,a)==R::Success,"fixed map binds an exact existing reservation to a subrange of backing");
    check(space.translate(Gpu::VirtualAddress{a.value+page-1},1,result) && result.object==11 && result.offset==2*page-1,"last translated byte remains inside backing");
    check(space.map(12,page,0,page,fixed,b)==R::Occupied,"fixed duplicate cannot replace an active mapping");
    const auto snapshot=space;check(space.unmap(a)==R::Success && space.count()==1 && !space.mappingCount() && snapshot.references(11),"staged mapping copies are independent and fixed unmap preserves the reservation");
    check(space.release(fixed,page)==R::Success && !space.count(),"fixed reservation can be freed after unmap");
    for(const auto offset:{uint64_t(1),2*page,UINT64_MAX})check(space.map(1,2*page,offset,page,{},a)==R::InvalidArguments,"unaligned, excessive and overflowing backing offsets fail");
    check(space.map(0,page,0,page,{},a)==R::InvalidArguments && space.map(1,page,0,0,{},a)==R::InvalidArguments,"invalid identity and zero extent fail");
    check(space.map(1,page,0,page,Gpu::VirtualAddress{S::Base},a)==R::InvalidAddress,"fixed mapping never silently creates an unreserved address");
    std::array<Gpu::VirtualAddress,S::Limit> addresses{};for(auto& address:addresses)check(space.map(3,page,0,page,{},address)==R::Success,"mapping quota can be filled with aliases");
    check(space.map(3,page,0,page,{},a)==R::OutOfResource,"mapping/range quotas are bounded");
    for(const auto address:addresses)check(space.unmap(address)==R::Success,"all aliases unmap cleanly");
    check(!space.references(3) && !space.count() && !space.mappingCount(),"final unmap leaves no reservations or object references");
    return failures?1:0;
}
