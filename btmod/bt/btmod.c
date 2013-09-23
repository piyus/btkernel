#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/percpu.h>
#include <asm/desc.h>
#include <asm-generic/vmlinux.lds.h>
#include "btmod.h"
#include "peep/peep.h"
#include "peep/cpu_constraints.h"
#include "peep/tb.h"
#include "peep/jumptable2.h"
#include "peep/jumptable1.h"
//#include "peep/gvtable.h"
#include "peep/i386-dis.h"
#include "peepgen_offsets.h"
#include "debug.h"
#include "sys/vcpu.h"
//#include "peep/tb_exit_callbacks.h"
#include "bt_vcpu.h"
#include "hypercall.h"
#include "btfix.h"
#include "instrument.h"
//#include <linux/btprintk.h>
//#include "modconfig.h"

MODULE_LICENSE("GPL");

/* GLOBAL VARIABLE DECLARATIONS */
#define OUT_BUF_SIZE 10000
btmod *bt;
int n_flag_opt = 0;
int n_reg_opt = 0;
int n_remapped_eips = 0;
bool shadow_idt_loaded = false;

struct desc_ptr shadow_idt_descr;
struct desc_ptr native_idt_descr;
uint64_t native_sysenter;
uint64_t target_sysenter;

char unique_ins[100][8];
int unique_ins_count = 0;

struct shared_page {
	unsigned long long arr[PAGE_SIZE/sizeof(unsigned long)];
} __aligned(PAGE_SIZE);

struct shared_page sp;

DEFINE_PER_CPU(uint8_t, read_mask_b);
DEFINE_PER_CPU(uint8_t, write_mask_b);
DEFINE_PER_CPU(uint16_t, read_mask_w);
DEFINE_PER_CPU(uint16_t, write_mask_w);
DEFINE_PER_CPU(uint32_t, read_mask_l);
DEFINE_PER_CPU(uint32_t, write_mask_l);

DEFINE_PER_CPU(uint8_t, unmask_b);
DEFINE_PER_CPU(uint16_t, unmask_w);
DEFINE_PER_CPU(uint32_t, unmask_l);

uint8_t runmask_b = 0xaa;
uint16_t runmask_w = 0xaaaa;
uint32_t runmask_l = 0xaaaaaaaa;

DEFINE_PER_CPU(unsigned long, tx_target);
DEFINE_PER_CPU(unsigned long, gsptr);
DEFINE_PER_CPU(unsigned long, tsptr);
DEFINE_PER_CPU(unsigned long, vptr);

#include "peep/peeptab_defs.h"
char __print_buf[512];
char __new_buf[2][512];
spinlock_t print_lock;

#ifndef SHADOWKERNEL
unsigned _shadowmem_start = 0;
#else
extern int bt_debug_printk;
#endif

/* btmod ioctl functions */

static void 
bt_load_idt(const struct desc_ptr *dtr)
{
  asm volatile("lidt %0"::"m" (*dtr));
}

static int
load_native_idt(int idx)
{
  uint32_t lo;
  lo = native_sysenter & 0xffffffff;
  bt_load_idt(&native_idt_descr);
  wrmsr(MSR_IA32_SYSENTER_EIP, lo, 0);
	return 0;
}

static int
translate_idt(int idx)
{
  /*static int __wait_idx = 0;
	int wait_idx;*/
  uint32_t lo;

	/*wait_idx = (idx == 0)?bt->smp_cpus-1:bt->smp_cpus;
	if (idx != 0) {
    __wait_idx++;
	}
	dprintk("vcpu %x waiting\n", idx);
	while (__wait_idx != wait_idx);*/
  local_irq_disable();
  /*if (idx == 0) {
    patch_translated_eips(bt->v);
    __wait_idx++;
	}*/
	bt_load_idt(&shadow_idt_descr);
	if (target_sysenter) {
    lo = target_sysenter & 0xffffffff;
    wrmsr(MSR_IA32_SYSENTER_EIP, lo, 0);
	}
	shadow_idt_loaded = true;
  local_irq_enable();
	return 0;
}

static void
dump_shadow_memory(void)
{
	if (_shadowmem_start) {
    outl(_shadowmem_start, DUMP_PORT);
	}
}

static void
reset_shadow_memory(void)
{
	if (_shadowmem_start) {
    outl(_shadowmem_start, RESET_PORT);
		//notify_gtable_addr();
	}
}

static int __inited = 0;

static void
free_module_extable(void)
{
  struct module *mod = THIS_MODULE;
  struct exception_table_entry *extable;
	size_t size;

	extable = mod->extable;
	size = sizeof(struct exception_table_entry) * MAX_EXTABLE_EIPS;
	printk("entries in exception table=%d\n", mod->num_exentries);
	mod->num_exentries = 0;
  btfree((void*)extable, size);
}

static int
alloc_module_extable(void)
{
  struct module *mod = THIS_MODULE;
  struct exception_table_entry *extable;
	int i;
	size_t size;

	size = sizeof(struct exception_table_entry) * MAX_EXTABLE_EIPS;
  extable = (struct exception_table_entry*)btalloc(size, PAGE_KERNEL);
	if (extable == NULL) {
		return 0;
	}
	for (i = 0; i < mod->num_exentries; i++) {
		extable[i].insn = mod->extable[i].insn;
	}
	mod->extable = extable;
	return 1;
}

static void
print_stats(int smp_cpus)
{
	int i, j;
	printk("n_flag_opt=%d n_reg_opt=%d n_remapped_eips=%d\n", 
			n_flag_opt, n_reg_opt, n_remapped_eips);
	for (i = 0; i < smp_cpus; i++) {
		for (j = 0; j < 4; j++) {
		  printk("cpu=%d shadowregs[%d]=%llx\n", 
					i, j, per_cpu(shadowregs[j], i));
		}
	}
	for (i = 0; i < unique_ins_count; i++) {
		printk("unique_ins[%d]=%s\n", i, unique_ins[i]);
	}
}

static int
stop_bt(int smp_cpus)
{
	int i;
	btmod_vcpu *v;

  push_orig_eips(bt);
	for (i = 0; i < smp_cpus; i++) {
		v = bt->v + i;
		free_pages(v->mon_stack, 1);
	}

  malloc_uninit(&bt->info);
	free_pages((unsigned long)bt->tpage, 4);

	peep_uninit(bt);
	free_module_extable();
	free_jumptable1();

	btfree((void*)bt->v, sizeof(btmod_vcpu) * smp_cpus);
	btfree((void*)bt, sizeof(btmod));

  print_stats(smp_cpus);
	return 0;
}

static void
init_shadow_data(int smp_cpus)
{
  int i;
  int r;

#ifdef SHADOWKERNEL
	init_task.tid = 1;
	bt_debug_printk = 1;
#endif

	for (i = 0; i < smp_cpus; i++) {
	  for (r = 0; r < NUM_SHADOWREGS; r++) {
	  	per_cpu(shadowregs[r], i) = 0;
	  }
	  if (_shadowmem_start) {
      r = 2 * i;
	  	per_cpu(read_mask_b, i) = 1 << r;
	  	per_cpu(write_mask_b, i) = 3 << r;
	  	per_cpu(read_mask_w, i) = (1 << r) | (1 << (r+8));
	  	per_cpu(write_mask_w, i) = (3 << r) | (3 << (r+8));
	  	per_cpu(read_mask_l, i) = (1 << r) | (1 << (r+8)) | (1 << (r+16)) | (1 << (r+24));
	  	per_cpu(write_mask_l, i) = (3 << r) | (3 << (r+8)) | (3 << (r+16)) |(3 << (r+24));
	  	per_cpu(unmask_b, i) = ~(3 << r);
	  	per_cpu(unmask_w, i) = ~((3 << r) | (3 << (r+8)));
	  	per_cpu(unmask_l, i) = ~((3 << r) | (3 << (r+8)) | (3 << (r+16)) |(3 << (r+24)));
	  }
	}
}

static int
init_bt_vcpu(btmod *bt, int smp_cpus)
{
	int i;
	btmod_vcpu *v;
  vcpu_t *vcpu;

	for (i = 0; i < smp_cpus; i++) {
		v = bt->v + i;
    vcpu = &v->vcpu;
	  v->bt = bt;
	  v->vcpu_id = i;

	  v->mon_stack = __get_free_pages(GFP_KERNEL, 1);
	  if (!v->mon_stack) {
			return 0;
		}

	  per_cpu(tsptr, i) = v->mon_stack + (2 * PAGE_SIZE);
	  per_cpu(vptr, i) = (unsigned long)v;

	  init_shadow_data(i);
    init_callout_label(vcpu);
    vcpu->prev_tb = 0;
	  vcpu_set_log(VCPU_LOG_TRANSLATE);
	}
	return 1;
}

static int
init_bt(int smp_cpus)
{
	int size;

#ifdef USER_EXTABLE
	if (!extable_is_inited()) {
		goto fail;
	}
#endif
	if (__inited) {
		printk("FATAL: BTMOD already initialised\n");
		goto fail;
	}
	__inited = 1;

	bt = (btmod*)btalloc(sizeof(btmod), PAGE_KERNEL);
  if (bt == NULL) {
		MEMPANIC();
    goto fail;
	}
	size = sizeof(btmod_vcpu) * smp_cpus;
	bt->v = (btmod_vcpu*)btalloc(size, PAGE_KERNEL);
	if (bt->v == NULL) {
		MEMPANIC();
		goto fail;
	}
	if (!alloc_jumptable1()) {
		MEMPANIC();
		goto fail;
	}
	clear_jumptable1(smp_cpus);

  bt->smp_cpus = smp_cpus;
	spin_lock_init(&print_lock);
	spin_lock_init(&bt->translation_lock);

	if (!peep_init(bt)) {
		MEMPANIC();
    goto fail;
	}

	bt->tpage = (void*)__get_free_pages(GFP_KERNEL, 4);
	if (bt->tpage == NULL) {
		MEMPANIC();
		goto fail;
	}
	if (!malloc_init(&bt->info)) {
		MEMPANIC();
		goto fail;
	}

  jumptable2_init(&bt->jumptable2);

  tb_init(&bt->tc_tree);

	if (!alloc_module_extable()) {
		MEMPANIC();
		goto fail;
	}

	if (!init_bt_vcpu(bt, smp_cpus)) {
		MEMPANIC();
		goto fail;
	}
	opc_init(bt->v);
	create_shadow_idt(bt->v);
	//clear_gvtable();
	printk("init BT sucessful %s(%p)\n", __func__, init_bt);
	return 0;

fail:
	printk("init BT failed.\n");
	return 1;
}

static long
device_ioctl(struct file *file,
		unsigned int ioctl, unsigned long arg)
{
  void __user *argp;
  unsigned long long insncnt[5];
	struct extable_ioctl e;
  int i;
	int ret = 0;

	switch (ioctl) {
		case STOP_BT:
			dump_shadow_memory();
			stop_bt(arg);
			break;
		case INIT_BT:
			ret = init_bt(arg);
			reset_shadow_memory();
			break;
		case BT_SET_MODE:
			break;
		case TRANSLATE_IDT:
			translate_idt(arg);
			break;
		case LOAD_NATIVE_IDT:
			load_native_idt(arg);
			break;
		case TRANSLATE_DEBUG_CODE:
			break;
		case READ_SHADOW_REG:
      argp = (void __user *)arg;
			ASSERT(__inited);
      for (i = 0; i <= 4; i++) {
        insncnt[i] = __get_cpu_var(shadowregs[i]);
      }
      ret = copy_to_user(argp, insncnt, sizeof(insncnt));
      break;
		case INIT_EXTABLE:
			argp = (void __user *)arg;
			if (copy_from_user((void*)&e, argp, sizeof(e))) {
				return 0;
			}
      argp = (void __user *)(e.addr);
			ret = init_exception_table(argp, e.len);
      break;
		default:
			printk("NOT a valid ioctl\n");
			ret = 1;
	}
	return ret;
}

static struct file_operations fops = {
	.unlocked_ioctl = device_ioctl,
};

int
start_translator(void)
{
	if (register_chrdev(MAJOR_NUM, MODULE_NAME, &fops) < 0) {
		printk("PANIC: BT module loading failed\n");
		return 1;
	}
	printk("BT module loaded sucessfully\n");
	return 0;
}

void
exit_translator(void)
{
	unregister_chrdev(MAJOR_NUM, MODULE_NAME);
}

module_init(start_translator);
module_exit(exit_translator);
