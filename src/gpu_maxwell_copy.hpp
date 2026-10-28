#pragma once
#include "gpu_classes.hpp"
namespace Gpu {
inline constexpr uint32_t CopyByteLimit=0x10000;
inline constexpr uint32_t CopyRowLimit=64;
struct MaxwellCopyState {
    uint32_t sourceUpper=0,sourceLower=0,destinationUpper=0,destinationLower=0,length=0;
    uint32_t sourcePitch=0,destinationPitch=0,rows=0;
    uint32_t sourceBlockSize=0,sourceWidth=0,sourceHeight=0,sourceDepth=0,sourceLayer=0,sourceOrigin=0;
    uint8_t configured=0;
    uint8_t sourceBlockConfigured=0;
    bool operator==(const MaxwellCopyState&) const=default;
};
DecodeResult executeMaxwellCopy(const MethodWrite& method,ChannelExecution& execution);
// Restricted Fermi 64-byte by 8-row GOB layout, one GOB per block height/depth.
inline uint64_t copyBlockSourceOffset(uint32_t width,uint32_t x,uint32_t y){
    return (uint64_t(y/8)*(width/64)+x/64)*512+(x%64/32)*256+(y%8/2)*64+(x%32/16)*32+(y%2)*16+x%16;
}
}
