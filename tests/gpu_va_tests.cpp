#include "gpu_va.hpp"
#include <iostream>
int main(){
    using Space=Gpu::AddressSpace;using R=Space::Result;unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}};
    Space space;Gpu::VirtualAddress a{7},b{9},c{11};constexpr auto page=Space::PageSize;
    check(space.reserve(2*page,page,{},a)==R::Success && a.value==Space::Base,"first reservation is a GPU address above CPU RAM");
    check(space.reserve(page,16*page,{},b)==R::Success && b.value% (16*page)==0,"automatic reservations honor larger power-of-two alignment");
    check(space.reserve(page,page,{},c)==R::Success && c.value==a.value+2*page,"first fit uses a gap before a later aligned reservation");
    const auto count=space.count(),bytes=space.reservedBytes();Gpu::VirtualAddress sentinel{123};
    check(space.reserve(page,page,a,sentinel)==R::Occupied && sentinel.value==123 && space.count()==count && space.reservedBytes()==bytes,"overlap rejection preserves output and registry");
    for(const auto invalid:{uint64_t(0),page-1,Space::Capacity+page,UINT64_MAX})check(space.reserve(invalid,page,{},sentinel)==R::InvalidArguments && sentinel.value==123,"invalid byte arithmetic cannot publish a reservation");
    for(const auto invalid:{uint64_t(0),page-1,page+1,UINT64_MAX})check(space.reserve(page,invalid,{},sentinel)==R::InvalidArguments,"unsupported alignment is rejected");
    check(space.release(a,page)==R::InvalidArguments && space.count()==count,"partial free cannot discard a larger reservation");
    check(space.release(Gpu::VirtualAddress{a.value+page},page)==R::InvalidAddress,"interior free cannot split a reservation");
    check(space.release(a,2*page)==R::Success && space.release(a,2*page)==R::InvalidAddress,"exact free works once");
    check(space.reserve(page,page,{},sentinel)==R::Success && sentinel.value==Space::Base,"freed holes can be reused deterministically");
    Space independent;Gpu::VirtualAddress other{};check(independent.reserve(page,page,{},other)==R::Success && other.value==Space::Base,"independent spaces own separate virtual namespaces");
    const auto snapshot=space;check(space.release(sentinel,page)==R::Success && snapshot.count()==space.count()+1,"staged copies retain independent allocation state");
    Space full;check(full.reserve(Space::Capacity,page,{},a)==R::Success && full.reserve(page,page,{},b)==R::OutOfResource,"virtual byte capacity is bounded independently of CPU backing");
    check(full.release(a,Space::Capacity)==R::Success,"full virtual region can be released");
    std::array<Gpu::VirtualAddress,Space::Limit> addresses{};for(auto& address:addresses)check(full.reserve(page,page,{},address)==R::Success,"bounded reservation slots fill successfully");
    check(full.reserve(page,page,{},b)==R::OutOfResource && full.count()==Space::Limit,"reservation count quota is enforced");
    check(full.release(addresses[7],page)==R::Success && full.reserve(page,page,{},b)==R::Success && b.value==addresses[7].value,"freed record and middle hole are reusable");
    for(const auto value:{uint64_t(0x200000),Space::Base-1,Space::Base+Space::Capacity,UINT64_MAX})check(full.reserve(page,page,Gpu::VirtualAddress{value},b)==R::InvalidArguments,"CPU, unaligned, out-of-range and overflowing fixed addresses are rejected");
    return failures?1:0;
}
