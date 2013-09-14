#include "peep/jumptable1.h"
#include <debug.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "bt_vcpu.h"
#include "hypercall.h"

unsigned long long __percpu *jumptable1;
unsigned long long __percpu *jumptable_aux[3];

void
jumptable1_add(uint32_t eip, uint32_t tc_ptr)
{
	int cpu = smp_processor_id();
  int indx = eip & JUMPTABLE1_MASK;
	uint64_t *jumptable = per_cpu_ptr(jumptable1, cpu);
#define JUMPTABLE_OPTIMIZATION 1
#ifdef JUMPTABLE_OPTIMIZATION
  uint64_t prev_val1 = jumptable[indx];

  if (prev_val1) {
    uint64_t prev_val2 = jumptable[indx+1];

    if (prev_val2) {
      uint64_t prev_val3 = jumptable[indx+2];

      if (prev_val3) {
        jumptable[indx+3] = prev_val3;
      }
      jumptable[indx+2] = prev_val2;
    }
    jumptable[indx+1] = prev_val1;
  }
#endif
  jumptable[indx] = (uint64_t)tc_ptr << 32 | eip;
}

void
clear_jumptable1(int smp_cpus)
{
	int i;
  size_t size;
	uint64_t *jumptable;
	size = JUMPTABLE1_SIZE * sizeof(unsigned long long);
	for (i = 0; i < smp_cpus; i++) {
	  jumptable = per_cpu_ptr(jumptable1, i);
	  memset(jumptable, 0, size);
	}
}

static inline bool
continuous_blocks(uint32_t addr1, uint32_t addr2, uint32_t size)
{
  if (addr1 + size != addr2) {
		MEMPANIC();
		printk("add1=%x add2=%x size=%d\n", addr1, addr2, size);
		return false;
	}
	return true;
}

/* 
 * FIXME: On new linux kernel version large per-cpu area
 * can not be allocated in one go. Dirty way to alloc
 * large continuous per-cpu area.
 */

int
alloc_jumptable1(void)
{
	size_t size, newsize;

  size = (1024 * sizeof(unsigned long long));
	jumptable1 = __alloc_percpu(size, __alignof__(unsigned long long));
  if (jumptable1 == NULL) {
		MEMPANIC();
    goto out_free_0;
	}

	jumptable_aux[0] = __alloc_percpu(size, __alignof__(unsigned long long));
  if (jumptable_aux[0] == NULL) {
		MEMPANIC();
    goto out_free_1;
	}
	if (!continuous_blocks((uint32_t)per_cpu_ptr(jumptable1, 0),
			  (uint32_t)per_cpu_ptr(jumptable_aux[0], 0), size)) {
    goto out_free_2;
	}

	jumptable_aux[1] = __alloc_percpu(size, __alignof__(unsigned long long));
  if (jumptable_aux[1] == NULL) {
		MEMPANIC();
    goto out_free_2;
	}
	if (!continuous_blocks((uint32_t)per_cpu_ptr(jumptable_aux[0], 0),
			  (uint32_t)per_cpu_ptr(jumptable_aux[1], 0), size)) {
    goto out_free_3;
	}

  newsize = (1028 * sizeof(unsigned long long));
	jumptable_aux[2] = __alloc_percpu(newsize, __alignof__(unsigned long long));
  if (jumptable_aux[2] == NULL) {
		MEMPANIC();
    goto out_free_3;
	}

	if (!continuous_blocks((uint32_t)per_cpu_ptr(jumptable_aux[1], 0),
			  (uint32_t)per_cpu_ptr(jumptable_aux[2], 0), size)) {
    goto out_free_4;
	}
	return 1;

out_free_4:
  free_percpu(jumptable_aux[2]);
out_free_3:
  free_percpu(jumptable_aux[1]);
out_free_2:
  free_percpu(jumptable_aux[0]);
out_free_1:
  free_percpu(jumptable1);
out_free_0:
  return 0;
}

void
free_jumptable1(void)
{
  free_percpu(jumptable1);
  free_percpu(jumptable_aux[0]);
  free_percpu(jumptable_aux[1]);
  free_percpu(jumptable_aux[2]);
}
