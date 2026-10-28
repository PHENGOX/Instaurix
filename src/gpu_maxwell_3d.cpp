#include "gpu_maxwell_3d.hpp"
namespace Gpu {
DecodeResult executeMaxwell3D(const MethodWrite& method,ChannelExecution&){
    // Zero is an explicit local payload restriction on the documented 0x100 NO_OPERATION.
    return method.method==0x100 && !method.value?DecodeResult::Success:DecodeResult::Unsupported;
}
}
