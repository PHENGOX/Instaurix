typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
#ifdef _MSC_VER
#include <math.h>
#define WORK_FMA fma
#else
#define WORK_FMA __builtin_fma
#endif
struct FpResult { float single; u32 positive; double wide; };
static const float singles[8] = {1.0f,-2.5f,0.125f,4.0f,-8.0f,3.25f,2.0f,-0.5f};
static const double doubles[8] = {-1.0,2.0,-4.5,8.0,-0.125,16.0,-32.0,0.25};
void fp_work(volatile struct FpResult* out)
{
    float single = 1.5f;
    double wide = -2.0;
    for (u32 i=0; i<8; ++i)
    {
        // Separate volatile products give identical, non-fused rounding on both CPUs.
        volatile float productSingle = single * 1.25f;
        volatile double productWide = wide * 0.5;
        single = productSingle + singles[i];
        wide = productWide + doubles[i];
    }
    out->single = single;
    out->positive = single > 0.0f ? 1 : 0;
    out->wide = wide;
}

struct ConversionResult {
    double fromSigned;
    float fromUnsigned;
    int backSigned;
    u64 backUnsigned;
    double widened;
    float narrowed;
    u32 padding;
    double fused;
};
void conversion_work(volatile struct ConversionResult* out, i64 signedInput, u64 unsignedInput)
{
    double d = (double)signedInput;
    float f = (float)unsignedInput;
    out->fromSigned = d;
    out->fromUnsigned = f;
    out->backSigned = (int)f;
    out->backUnsigned = (u64)(d * -0.5);
    out->widened = (double)f;
    out->narrowed = (float)d;
    out->padding = 0;
    out->fused = WORK_FMA(d,0.5,(double)unsignedInput);
}
