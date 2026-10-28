#pragma once
#include "gpu_submission.hpp"
namespace Gpu {
struct MethodWrite { size_t buffer=0,headerWord=0;uint32_t subchannel=0,method=0,value=0; };
struct DecodedPushbuffer { std::vector<MethodWrite> methods;size_t dmaNops=0; };
enum class DecodeResult { Success,Malformed,Unsupported,TransferLimit,OutOfResource };
// Historical B06F packet structure only; no method execution or job admission.
DecodeResult decodePushbuffer(const SubmissionSnapshot& input,DecodedPushbuffer& output);
}
