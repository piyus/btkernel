#ifndef __TYPES_H
#define __TYPES_H
#include <linux/types.h>

#ifdef __PEEPGEN__
#define dprintk printf
#define kmalloc umalloc
#define kfree free
#undef ASSERT
#define ASSERT assert
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
/* "Safe" macros for min() and max(). See [GCC-ext] 4.1 and 4.6. */
#define min(a,b)                                                      \
  ({typeof (a) _a = (a); typeof (b) _b = (b);                         \
    _a < _b ? _a : _b;                                                \
   })
#define max(a,b)                                                      \
  ({typeof (a) _a = (a); typeof(b) _b = (b);                          \
    _a > _b ? _a : _b;                                                \
   })


#define GFP_KERNEL 0
#define UINT_MAX 4294967295U

#define LONG_MAX 2147483647L
#define LONG_MIN (-LONG_MAX - 1)
#define ULONG_MAX 4294967295UL

#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1)
#define ULLONG_MAX 18446744073709551615ULL

#endif

/*typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;*/

typedef uint32_t target_ulong;
typedef uint32_t target_phys_addr_t;

/* Index of a disk sector within a disk.
   Good enough for disks up to 2 TB. */
//typedef uint32_t disk_sector_t;

#ifndef INT8_MAX
#define INT8_MAX 127
#define INT8_MIN (-INT8_MAX - 1)

#define INT16_MAX 32767
#define INT16_MIN (-INT16_MAX - 1)

#define INT32_MAX 2147483647
#define INT32_MIN (-INT32_MAX - 1)

#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-INT64_MAX - 1)

#define UINT8_MAX 255

#define UINT16_MAX 65535

#define UINT32_MAX 4294967295U

#define UINT64_MAX 18446744073709551615ULL

#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX

#define UINTPTR_MAX UINT32_MAX

#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX

#define UINTMAX_MAX UINT64_MAX

#define PTRDIFF_MIN INT32_MIN
#define PTRDIFF_MAX INT32_MAX

//#define SIZE_MAX UINT32_MAX

/**/
#endif


#endif
