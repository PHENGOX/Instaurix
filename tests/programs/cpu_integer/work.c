/* Shared C source: compiled to ARM64 for the guest, to native x64 for reference. */
typedef unsigned long long u64;
typedef unsigned int u32;
#if defined(_MSC_VER) && !defined(__clang__)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
const u64 seed_a = 0xFEDCBA9876543210ULL;
const u64 seed_b = 0x1234567ULL;
const u32 seed_c = 0xF1234567U;
const u32 seed_d = 23;
volatile u64 observed64;
volatile u32 observed32;

NOINLINE u64 work64(u64 a, u64 b)
{
    for (u32 i=0; i<8; ++i) {
        a ^= a >> 11;
        a = a*b + 17;
        a = (a << 9) | (a >> 55);
        a ^= a / (b|1);
        a += a % (b|1);
        a = a < b ? a ^ 0x55 : a+b;
        b = ((b >> 3) ^ a)|1;
    }
    return a;
}
NOINLINE u32 work32(u32 a, u32 b)
{
    for (u32 i=0; i<8; ++i) {
        a ^= a >> 7;
        a = a*b + 31;
        a = (a << 5) | (a >> 27);
        a ^= a / (b|1);
        a += a % (b|1);
        a = a < b ? a ^ 0xAA : a+b;
        b = ((b >> 3) ^ a)|1;
    }
    return a;
}
