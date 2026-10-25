typedef unsigned long long u64;
volatile u64 initial = 10;
const u64 expected = 55;
volatile u64 output;
volatile u64 zero_probe;

__attribute__((noinline)) u64 compute_sum(void)
{
    volatile u64 remaining = initial;
    u64 total = 0;
    do {
        total += remaining;
        remaining = remaining - 1;
    } while (remaining != 0);
    output = total;
    return total;
}
