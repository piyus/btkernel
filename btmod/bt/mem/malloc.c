#include "mem/malloc.h"
#include <asm/page.h>
#include <debug.h>
#include "lib/mylist.h"
#include "mem/vaddr.h"
#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/page.h>
#include "hypercall.h"

#define MAX_POOL_PAGES 64 /* please update corresponding log pages */
#define POOL_LOG_PAGES 6

uint32_t
btalloc(int size, pgprot_t prot)
{
  uint32_t addr, temp;

  size = PAGE_ALIGN(size);
  addr = (uint32_t)__vmalloc(size, GFP_KERNEL, prot);

  if (!addr) {
    return 0;
  }

  memset((void*)addr, 0, size);

  temp = addr;
  while (size > 0) {
    SetPageReserved(vmalloc_to_page((void *)temp));
    temp += PAGE_SIZE;
    size -= PAGE_SIZE;
  }

  return addr;
}

void 
btfree(void *ptr, int size)
{
  uint32_t addr;

  if (!ptr) {
    return;
  }

  addr = (uint32_t)ptr;
  while (size > 0) {
    ClearPageReserved(vmalloc_to_page((void *)addr));
    addr += PAGE_SIZE;
    size -= PAGE_SIZE;
  }
  vfree(ptr);
}

void *
malloc_from_pool (struct malloc_info *info, unsigned pool, size_t size) 
{
	int i;
  int start = info->pool_range[pool][0];
  int end = info->pool_range[pool][1];
  for (i = start; i < end; i++) {
		if (info->cinfo[i].chunk_cur + size <= info->cinfo[i].chunk_end) {
      void *ret = (void*)(info->cinfo[i].chunk_cur);
      info->cinfo[i].chunk_cur += size;
			//dprintk("chunk=%d addr=%p size=%d\n", i, ret, size);
			return ret;
		}
	}
  printk("pool=%d start=%d end=%d size=%d\n", pool, start, end, size);
  printk("Pool is FULL %x\n", pool);
  BUG();
	return NULL;
}

static int
pool_alloc_pages(struct malloc_info *info, int pool)
{
	int i;
	uint32_t ptr;
	for (i = 0; i < MAX_CHUNK; i++) {
		ptr = btalloc(MAX_POOL_PAGES * PAGE_SIZE, PAGE_KERNEL_EXEC);
    if (ptr == 0) {
			return 0;
		}
		info->cinfo[i].chunk_start = ptr;
		info->cinfo[i].chunk_cur = ptr;
		/* FIXME: sprint use 16 bytes input buffer hence -16 */
		info->cinfo[i].chunk_end = ptr + (MAX_POOL_PAGES * PAGE_SIZE) - 16;
    dprintk("pool=%d chunk=%d start=%x end=%x\n", 
				pool, i, ptr, info->cinfo[i].chunk_end);
	}
	return 1;
}

static int
addr_to_chunk(struct malloc_info *info, uint32_t addr)
{
  int i;
  for (i = 0; i < MAX_CHUNK; i++) {
    if (  addr >= info->cinfo[i].chunk_start
      && addr < info->cinfo[i].chunk_end) {
       return i;
    }
  }
  return -1;
}

int
addr_to_pool(struct malloc_info *info, uint32_t addr)
{
  int chunk_id;
  chunk_id = addr_to_chunk(info, addr);
  if (chunk_id >= 0 && chunk_id < 24) {
    return POOL_TC;
  } else if (chunk_id >= 24 && chunk_id < 40) {
    return POOL_EDGE;
  }
  return -1;
}

void
malloc_uninit(struct malloc_info *info)
{
	int i;
	for (i = 0; i < MAX_CHUNK; i++) {
		btfree((void*)info->cinfo[i].chunk_start, MAX_POOL_PAGES * PAGE_SIZE);
	}
}

static void
init_pool_range(struct malloc_info *info)
{
  info->pool_range[POOL_TC][0] = 0;
  info->pool_range[POOL_TC][1] = 24;
  info->pool_range[POOL_EDGE][0] = 24;
  info->pool_range[POOL_EDGE][1] = 40;
  info->pool_range[POOL_KMALLOC][0] = 40;
  info->pool_range[POOL_KMALLOC][1] = 60;
}

int
malloc_init (struct malloc_info *info) 
{
	if (pool_alloc_pages(info, 0) == 0) {
		return 0;
	}
  init_pool_range(info);
	return 1;
}
