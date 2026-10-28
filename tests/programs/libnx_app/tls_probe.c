#include <stdint.h>
/* Application-owned TLS probes; upstream library sources remain unchanged. */
__thread uint64_t reference_tls_seed __attribute__((section(".tdata.reference_seed"))) = 0x1122334455667788ULL;
__thread uint64_t reference_tls_zero;
uint64_t referenceTlsReadSeed(void) { return reference_tls_seed; }
uint64_t referenceTlsReadZero(void) { return reference_tls_zero; }
void referenceTlsWrite(uint64_t seed, uint64_t zero) {
    reference_tls_seed = seed;
    reference_tls_zero = zero;
}
