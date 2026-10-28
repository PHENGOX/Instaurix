/* Build compatibility for the original GCC-oriented HID client under Clang.
 * HID shared-memory fields are ordinary integer types, not C11 _Atomic types.
 * Use Clang's GCC-compatible acquire-load builtin, preserving real load ordering.
 * This header is injected only when compiling the unmodified upstream hid.c.
 */
#include <stdint.h>
#include <stdatomic.h>
#undef atomic_load_explicit
#define atomic_load_explicit(address, order) __extension__ ({ \
    __auto_type __hid_atomic_pointer = (address); \
    __typeof__((0, *__hid_atomic_pointer)) __hid_atomic_value; \
    __atomic_load(__hid_atomic_pointer, &__hid_atomic_value, (order)); \
    __hid_atomic_value; \
})
