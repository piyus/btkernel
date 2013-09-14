#ifndef THREADS_MALLOC_H
#define THREADS_MALLOC_H

#include <debug.h>
#include <stddef.h>
#include <mylist.h>
#include <types.h>
#ifndef __PEEPGEN__
#include <linux/module.h>
#endif

#define POOL_TC         0
#define POOL_EDGE       1
#define POOL_KMALLOC    2
#define NUM_POOLS       3
#define MAX_CHUNK       60

struct chunk_info {
	uint32_t chunk_start;
	uint32_t chunk_end;
  uint32_t chunk_cur;
};

struct malloc_info {
  int pool_range[NUM_POOLS][2];
	struct chunk_info cinfo[MAX_CHUNK];
};

int malloc_init (struct malloc_info *info);
void malloc_uninit(struct malloc_info *info);
void *malloc_from_pool(struct malloc_info *info, 
		unsigned pool_id, size_t size) __attribute__ ((malloc));
int addr_to_pool(struct malloc_info *info, uint32_t addr);
#ifndef __PEEPGEN__

uint32_t btalloc(int size, pgprot_t prot);
void btfree(void *ptr, int size);

#else

#define btalloc(size, prot) malloc(size)
#define btfree(ptr, size) free(ptr)

#endif

#endif /* threads/malloc.h */
