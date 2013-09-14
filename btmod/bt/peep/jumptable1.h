#ifndef PEEP_JUMPTABLE1_H
#define PEEP_JUMPTABLE1_H
#include <linux/types.h>

#define JUMPTABLE1_SIZE 4096
#define JUMPTABLE1_MASK (JUMPTABLE1_SIZE - 1)
struct btmod_vcpu;
void jumptable1_add(uint32_t eip, uint32_t tc_ptr);
void clear_jumptable1(int smp_cpus);
int alloc_jumptable1(void);
void free_jumptable1(void);

#endif
