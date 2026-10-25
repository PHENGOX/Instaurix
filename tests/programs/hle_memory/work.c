typedef unsigned long long u64;
const u64 expected = 55;
volatile u64 result;
volatile u64 last_tls;

__attribute__((noinline)) void fill_heap(volatile u64* heap, u64 tls)
{
    volatile u64 remaining = 10;
    u64 total = 0;
    do {
        total += remaining;
        remaining = remaining - 1;
    } while (remaining != 0);
    heap[0] = total;
    *(volatile u64*)(tls + 0x180) = total;
    result = total;
    last_tls = tls;
}
