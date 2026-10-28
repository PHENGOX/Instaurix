#pragma once
#include "gpu_classes.hpp"
namespace Gpu {
// B197 subset only; no drawing, shader execution or image effects.
DecodeResult executeMaxwell3D(const MethodWrite& method,ChannelExecution& execution);
}
