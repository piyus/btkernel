#ifndef BT_VCPU_H
#define BT_VCPU_H

#include "peep/tb.h"
#include "peep/i386-dis.h"
#include "sys/vcpu.h"
#include "peep/peep.h"

#define MAX_CPUS 32
#define JUMPTABLE1_SIZE 4096

#ifndef __PEEPGEN__
#include "mem/malloc.h"
#include <linux/module.h>
#include <asm/page.h>
#include <asm/desc.h>
#include <linux/list.h>
#include <asm/processor.h>
#include <asm/atomic64_32.h>


#define NUM_SHADOWREGS   8
#define MAX_EXTABLE_EIPS 4096
#define MAX_PATCHED_EIPS 1000

DECLARE_PER_CPU(unsigned long, tx_target);
DECLARE_PER_CPU(unsigned long, vptr);
DECLARE_PER_CPU(unsigned long, gsptr);
DECLARE_PER_CPU(unsigned long long[NUM_SHADOWREGS], shadowregs);
extern unsigned long long __percpu *jumptable1;

DECLARE_PER_CPU(uint8_t, read_mask_b);
DECLARE_PER_CPU(uint8_t, write_mask_b);
DECLARE_PER_CPU(uint16_t, read_mask_w);
DECLARE_PER_CPU(uint16_t, write_mask_w);
DECLARE_PER_CPU(uint32_t, read_mask_l);
DECLARE_PER_CPU(uint32_t, write_mask_l);

DECLARE_PER_CPU(uint8_t, unmask_b);
DECLARE_PER_CPU(uint16_t, unmask_w);
DECLARE_PER_CPU(uint32_t, unmask_l);

extern struct desc_ptr shadow_idt_descr;
extern struct desc_ptr native_idt_descr;
extern spinlock_t print_lock;
extern uint64_t native_sysenter;
extern uint64_t target_sysenter;
extern int n_flag_opt;
extern int n_reg_opt;
extern char unique_ins[100][8];
extern int unique_ins_count;
extern int n_remapped_eips;
extern bool shadow_idt_loaded;
#ifndef SHADOWKERNEL
extern unsigned _shadowmem_start;
#endif

enum status {
  NONE,
  PATCHED,
  UNPATCHED,
};

#define MALLOC_ERROR \
	dprintk("FATAL: kmalloc %s %d\n", __func__, __LINE__);\
  return 1

#endif

#ifdef __PEEPGEN__
#define PAGE_SIZE 4096
#define atomic64_t uint64_t
#endif

struct btmod;
struct btmod_opctable;

typedef struct btmod_vcpu {
	struct btmod *bt;
	bti386_t bti386;
	bt_peep_t bt_peep;
	vcpu_t vcpu;
	int vcpu_id;
	unsigned long long *jumptable;
	unsigned long mon_stack;
} btmod_vcpu;

typedef struct btmod {
	struct btmod_opctable ot;
	struct hash peep_tab;
#ifndef __PEEPGEN__
	struct malloc_info info;
  struct rbtree jumptable2;
  struct rbtree tc_tree;
  spinlock_t translation_lock;
#endif
	btmod_vcpu *v;
	void *tpage;
  int smp_cpus;
} btmod;

static inline uint64_t rdtsc(void)
{
	uint32_t hi, lo;
	asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
	return lo | (uint64_t)hi << 32;
}

void create_shadow_idt(btmod_vcpu *v);
void init_callout_label(vcpu_t *vcpu);
void patch_translated_eips(struct btmod_vcpu *v);

#endif
