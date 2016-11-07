#ifndef __UKVM_PRIVATE_H__
#define __UKVM_PRIVATE_H__

#define GUEST_SIZE      0x20000000 /* 512 MBs */
#define GUEST_PAGE_SIZE 0x200000   /* 2 MB pages in guest */

#define ALIGN_UP(_num, _align)    (_align * ((_num + _align - 1) / _align))

/*
 * Define uaddll_overflow(a, b, result) for compilers without
 * __builtin_uaddll_overlow().  Based on "Catching Integer Overflows in C"
 * (https://www.fefe.de/intof.html).
 */
#if defined(__GNUC__)
#if __GNUC__ >= 5
#define _HAS_BUILTIN_OVERFLOW
#endif
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_uaddll_overflow)
#define _HAS_BUILTIN_OVERFLOW
#endif

#ifdef _HAS_BUILTIN_OVERFLOW
#define uaddll_overflow(a,b,r) __builtin_uaddll_overflow(a,b,&r)
#else
#define __HALF_MAX_SIGNED(type) ((type)1 << (sizeof(type) * 8 - 2))
#define __MAX_SIGNED(type) (__HALF_MAX_SIGNED(type) - 1 + __HALF_MAX_SIGNED(type))
#define __MIN_SIGNED(type) (-1 - __MAX_SIGNED(type))

#define __MIN(type) ((type)-1 < 1 ? __MIN_SIGNED(type) : (type)0)
#define __MAX(type) ((type)~__MIN(type))

#define __assign(dest,src)                                                     \
    ({                                                                         \
        __typeof(src) __x = (src);                                             \
        __typeof(dest) __y = __x;                                              \
        (__x == __y && ((__x < 1) == (__y < 1)) ? (void)((dest) = __y),0 : 1); \
    })

#define uaddll_overflow(a,b,r)                                                  \
    ({                                                                         \
    __typeof(a) __a = a;                                                       \
    __typeof(b) __b = b;                                                       \
    ((__MAX(__typeof(r)) - (__b) >= (__a)) ? __assign(r, __a + __b) : 1);      \
    })

#endif
#undef _HAS_BUILTIN_OVERFLOW

/*
 * Given a pointer to 32-bit guest I/O write data, dereference and return as
 * a guest physical address (uint64_t).
 */
#define GUEST_PIO32_TO_PADDR(x) (uint64_t)(* (uint32_t *)(x))

/*
 * Given a guest physical address (p), validate that:
 *   - (p) is within the limit (l)
 *   - (p + sz) does not overflow and is within the limit (l)
 *
 * Note that (p), (l) and (sz) should be uint64_t or compatible types.
 */
#include <inttypes.h>
#define GUEST_CHECK_PADDR(p, l, sz)                                     \
    {                                                                          \
        uint64_t __e;                                                          \
        if ((p >= l) || uaddll_overflow(p, sz, __e) || (__e >= l))              \
            errx(1, "%s:%d: Invalid guest access: paddr=0x%"PRIu64 " sz=%lu",         \
                    __FILE__, __LINE__, p, sz);                                \
    }

#endif
