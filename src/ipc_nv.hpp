#pragma once
#include "ipc.hpp"
namespace Ipc {
struct NvIoctl { uint32_t fd=0,request=0;uint64_t source=0,destination=0;size_t size=0;uint32_t command=1;uint64_t extraSource=0;size_t extraSize=0; };
uint32_t parseNvIoctl(const Buffer& packet,Bus& bus,uint64_t ipcAddress,NvIoctl& plan);
}
