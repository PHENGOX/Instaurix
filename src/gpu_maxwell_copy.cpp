#include "gpu_maxwell_copy.hpp"
#include "gpu_channel.hpp"
namespace Gpu {
DecodeResult executeMaxwellCopy(const MethodWrite& method,ChannelExecution& execution){
    auto& registers=execution.state.copy[method.subchannel];
    switch(method.method){
    case 0x100:return !method.value?DecodeResult::Success:DecodeResult::Unsupported;
    case 0x400:if(method.value&~0xFFU)return DecodeResult::Malformed;registers.sourceUpper=method.value;registers.configured|=1;break;
    case 0x404:registers.sourceLower=method.value;registers.configured|=2;break;
    case 0x408:if(method.value&~0xFFU)return DecodeResult::Malformed;registers.destinationUpper=method.value;registers.configured|=4;break;
    case 0x40C:registers.destinationLower=method.value;registers.configured|=8;break;
    case 0x410:registers.sourcePitch=method.value;registers.configured|=32;break;
    case 0x414:registers.destinationPitch=method.value;registers.configured|=64;break;
    case 0x418:registers.length=method.value;registers.configured|=16;break;
    case 0x41C:registers.rows=method.value;registers.configured|=128;break;
    case 0x728:registers.sourceBlockSize=method.value;registers.sourceBlockConfigured|=1;break;
    case 0x72C:registers.sourceWidth=method.value;registers.sourceBlockConfigured|=2;break;
    case 0x730:registers.sourceHeight=method.value;registers.sourceBlockConfigured|=4;break;
    case 0x734:registers.sourceDepth=method.value;registers.sourceBlockConfigured|=8;break;
    case 0x738:registers.sourceLayer=method.value;registers.sourceBlockConfigured|=16;break;
    case 0x73C:registers.sourceOrigin=method.value;registers.sourceBlockConfigured|=32;break;
    case 0x300:{
        // NON_PIPELINED | FLUSH | SRC_PITCH | DST_PITCH, optionally MULTI_LINE, no remap.
        if(method.value!=0x186 && method.value!=0x386 && method.value!=0x306)return DecodeResult::Unsupported;
        const bool block=method.value==0x306;
        const bool multiline=method.value!=0x186;
        if((registers.configured&31)!=31 || !registers.length)return DecodeResult::Malformed;
        const unsigned required=block?223:255;
        if(multiline && ((registers.configured&required)!=required || !registers.rows || (!block && registers.sourcePitch<registers.length) || registers.destinationPitch<registers.length))return DecodeResult::Malformed;
        if(block){
            if(registers.sourceBlockConfigured!=63)return DecodeResult::Malformed;
            // First reference form: a complete aligned image, Fermi 8, one GOB block height/depth.
            if(registers.sourceBlockSize!=0x1000 || registers.sourceDepth!=1 || registers.sourceLayer || registers.sourceOrigin ||
               registers.sourceWidth!=registers.length || registers.sourceHeight!=registers.rows || registers.length%64 || registers.rows%8)return DecodeResult::Unsupported;
        }
        const auto rows=multiline?registers.rows:1U;
        const auto sourcePitch=multiline?registers.sourcePitch:registers.length;
        const auto destinationPitch=multiline?registers.destinationPitch:registers.length;
        const uint64_t sourceSpan=block?uint64_t(rows)*registers.length:uint64_t(rows-1)*sourcePitch+registers.length;
        const uint64_t destinationSpan=uint64_t(rows-1)*destinationPitch+registers.length;
        if(registers.length>CopyByteLimit || execution.copy)return DecodeResult::TransferLimit;
        // Local resource bounds apply both to transferred bytes and to each complete envelope.
        if(rows>CopyRowLimit || uint64_t(rows)*registers.length>CopyByteLimit || sourceSpan>CopyByteLimit || destinationSpan>CopyByteLimit)return DecodeResult::TransferLimit;
        const auto source=(uint64_t(registers.sourceUpper)<<32)|registers.sourceLower;
        const auto destination=(uint64_t(registers.destinationUpper)<<32)|registers.destinationLower;
        if(block && source%512)return DecodeResult::Malformed;
        if(source>=(1ULL<<40) || destination>=(1ULL<<40))return DecodeResult::Malformed;
        if(sourceSpan>(1ULL<<40)-source || destinationSpan>(1ULL<<40)-destination)return DecodeResult::Malformed;
        // Conservatively reject overlapping envelopes, including interleaved padding.
        if(source>=destination?source-destination<destinationSpan:destination-source<sourceSpan)return DecodeResult::Malformed;
        execution.copy=CopyRequest{VirtualAddress{source},VirtualAddress{destination},registers.length,rows,sourcePitch,destinationPitch,block};break;
    }
    default:return DecodeResult::Unsupported;
    }
    return DecodeResult::Success;
}
}
