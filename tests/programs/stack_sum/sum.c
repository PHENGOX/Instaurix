typedef unsigned long long u64;

// Volatile keeps a real loop and stack accesses in the compiled test.
__attribute__((noinline)) u64 sum_to(u64 n)
{
    volatile u64 remaining = n;
    u64 total = 0;
    do {
        total += remaining;
        remaining = remaining - 1;
    } while (remaining != 0);
    return total;
}
