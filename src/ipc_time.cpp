#include "ipc.hpp"
#include <chrono>

namespace Ipc {
uint32_t dispatchTime(const Buffer& request,Buffer& response,TimeRole role,uint64_t virtualNanoseconds,uint64_t epoch,TimeRequest& plan)
{
    Layout layout;
    const auto status=parseBuffers(request,nullptr,0,layout);
    if(status) return status;
    if(layout.sends || layout.receives) return UnsupportedFormat;
    auto read=[&](size_t at) { uint64_t value=0;for(size_t i=0;i<8;++i)value|=uint64_t(request[at+i])<<(8*i);return value; };
    TimeRequest next;
    if(role==TimeRole::Root && layout.command<=4) {
        if(layout.payloadSize) next.result=InvalidArguments;
        else next.child=static_cast<TimeRole>(layout.command+1);
    } else if((role==TimeRole::User || role==TimeRole::Network || role==TimeRole::Local || role==TimeRole::Steady) && layout.command==0) {
        if(layout.payloadSize) next.result=InvalidArguments;
        else if(role==TimeRole::Steady) next.steady=true;
        else next.current=true;
    } else if(role==TimeRole::Zone && layout.command==101) {
        if(layout.payloadSize!=8) next.result=InvalidArguments;
        else { next.timestamp=read(layout.payload);next.calendar=next.timestamp<=253402300799ULL;if(!next.calendar)next.result=InvalidArguments; }
    } else next.result=UnknownCommand;
    if(next.current && epoch>UINT64_MAX-virtualNanoseconds/1000000000ULL) { next.current=false;next.result=InvalidArguments; }
    managerResponse(response,next.result);
    auto put=[&](size_t at,uint64_t value,size_t size) { for(size_t i=0;i<size;++i)response[at+i]=uint8_t(value>>(8*i)); };
    if(next.current) valueResponse(response,0,epoch+virtualNanoseconds/1000000000ULL);
    if(next.steady) {
        response[4]=14;put(32,virtualNanoseconds/1000000000ULL,8);
        constexpr std::array<uint8_t,16> source{'S','W','2','E','M','U','_','T','I','M','E','_','R','E','F',1};
        for(size_t i=0;i<source.size();++i)response[40+i]=source[i];
    }
    if(next.calendar) {
        using namespace std::chrono;
        const sys_seconds point{seconds{static_cast<int64_t>(next.timestamp)}};
        const auto day=floor<days>(point);const year_month_day date{day};const hh_mm_ss tod{point-day};
        response[4]=16;
        put(32,unsigned(int(date.year())),2);put(34,unsigned(date.month()),1);put(35,unsigned(date.day()),1);
        put(36,tod.hours().count(),1);put(37,tod.minutes().count(),1);put(38,tod.seconds().count(),1);
        put(40,weekday{day}.c_encoding(),4);put(44,(day-sys_days{date.year()/January/1}).count(),4);
        response[48]='U';response[49]='T';response[50]='C'; // DST=0, UTC offset=0, padding remains zero.
    }
    plan=next;return 0;
}
}
