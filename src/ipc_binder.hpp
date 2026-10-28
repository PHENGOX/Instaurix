#pragma once
#include "ipc.hpp"
#include <memory>
namespace Ipc {
struct BinderTransaction {
    uint32_t object=0,code=0,flags=0;
    uint64_t source=0,sourceSize=0,destination=0,destinationSize=0;
};
uint32_t parseBinderTransaction(const Buffer& request,Bus& bus,uint64_t ipcAddress,BinderTransaction& plan);
uint32_t buildQueueReply(Bus& bus,const BinderTransaction& plan,bool connected,Transfer& transfer,bool& nextConnected);
struct QueueSurface {
    uint32_t object=0,offset=0,size=4096,width=16,height=16,pitch=64;
    bool blockLinear=false;
    std::array<uint8_t,364> descriptor{};
};
using QueueSurfaces=std::array<std::shared_ptr<const QueueSurface>,64>;
struct QueueRegistration { uint32_t slot=0,object=0;bool present=false;std::shared_ptr<const QueueSurface> surface; };
uint32_t parseQueueRegistration(Bus& bus,const BinderTransaction& plan,QueueRegistration& registration,Transfer& reply);
uint32_t buildQueueAccess(Bus& bus,const BinderTransaction& plan,const std::array<uint32_t,64>& slots,uint64_t acquired,Transfer& reply,uint64_t& nextAcquired,const QueueSurfaces& surfaces,uint32_t start=0);
}
