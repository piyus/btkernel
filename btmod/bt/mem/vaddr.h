#ifndef MEM_VADDR_H
#define MEM_VADDR_H

#include <debug.h>
#include <types.h>
#include <stdbool.h>

/* Functions and macros for working with virtual addresses.

   See pte.h for functions and macros specifically for x86
   hardware page tables. */

#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */

/* Large page offset (bits 0:22). */
#define LPGSHIFT 0                            /* Index of first offset bit. */
#define LPGBITS  22                           /* Number of offset bits. */
#define LPGSIZE  (1 << LPGBITS)               /* Bytes in a page. */
#define LPGMASK  BITMASK(LPGSHIFT, LPGBITS)   /* Page offset bits (0:22). */


/* Offset within a page. */
static inline unsigned pg_ofs (const void *va) {
  return (uintptr_t) va & PGMASK;
}

/* Offset within a page. */
static inline unsigned lpg_ofs (const void *va) {
  return (uintptr_t) va & LPGMASK;
}

/* Virtual page number. */
static inline uintptr_t pg_no (const void *va) {
  return (uintptr_t) va >> PGBITS;
}

/* Round up to nearest page boundary. */
static inline void *pg_round_up (const void *va) {
  return (void *) (((uintptr_t) va + PGSIZE - 1) & ~PGMASK);
}

/* Round down to nearest page boundary. */
static inline void *pg_round_down (const void *va) {
  return (void *) ((uintptr_t) va & ~PGMASK);
}

#endif /* mem/vaddr.h */
