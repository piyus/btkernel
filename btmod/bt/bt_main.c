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
#include <linux/random.h>
#include <linux/init_task.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/percpu.h>
#include <asm/desc.h>
#include <utils.h>
#include "btmod.h"
#include "peep/peep.h"
#include "peep/cpu_constraints.h"
#include "peep/tb.h"
#include "peep/jumptable2.h"
#include "peep/jumptable1.h"
#include "peep/i386-dis.h"
#include "peepgen_offsets.h"
#include "debug.h"
#include "sys/vcpu.h"
#include "peep/tb_exit_callbacks.h"
#include "bt_vcpu.h"
#include "hypercall.h"
#include "peep/peeptab_defs.h"
#include "peep/insn.h"
#include "peep/instrument.h"
#include "btfix.h"
#include "config.h"

//#define DEBUG_BT 1

MODULE_LICENSE("GPL");
DEFINE_PER_CPU(unsigned long long[NUM_SHADOWREGS], shadowregs);

struct bt_shadow {
	  gate_desc table[NR_VECTORS];
} __attribute__ ((aligned(PAGE_SIZE)));

struct bt_shadow shadow __section(.data..page_aligned);
static int lock_owner = 0xff;

static unsigned long
__tb_gen_translate_code(btmod_vcpu *v, uint32_t eip_virt)
{
  btmod *bt =v->bt; 
	tb_t *tb = NULL;
	unsigned long tc_offset = 0;

	tb = jumptable2_find(&bt->jumptable2, eip_virt);
	if (!tb) {
		tb = alloc_a_tb(bt);
    ASSERT(tb);
		tb->eip_virt = eip_virt;
    translate(v, tb, false);
		tb_malloc(bt, tb);
		translate(v, tb, true);
		jumptable2_add(&bt->jumptable2, tb);
    tb_add(&bt->tc_tree, tb);
//#ifdef DEBUG_BT
	  //print_asm_info(v, tb);
//#endif
	}

  if (tb->eip_virt == eip_virt) {
    tc_offset = (target_ulong)tb->tc_ptr;
  } else {
    tc_offset = eip_virt_to_tc_ptr(tb, eip_virt);
		tb_add_jump(bt, tb, 
			(uint16_t)(tc_offset - (uint32_t)tb->tc_ptr));
  }
	return tc_offset;
}

static void *
tb_gen_translated_code(btmod_vcpu *v)
{
  btmod *bt =v->bt; 
	vcpu_t *vcpu = &v->vcpu;
	uint32_t eip_virt;
	unsigned long tc_offset = 0;

	eip_virt = vcpu_get_eip(vcpu);
  spin_lock(&bt->translation_lock);
	lock_owner = v->vcpu_id;
	tc_offset = __tb_gen_translate_code(v, eip_virt);
  ASSERT(tc_offset);
	lock_owner = 0xff;
  spin_unlock(&bt->translation_lock);

	jumptable1_add(eip_virt, tc_offset);

  if (vcpu->prev_tb) {
    *((target_ulong *)vcpu->prev_tb) = 
       tc_offset - (vcpu->prev_tb + sizeof(target_ulong));
  }
	return (void*)tc_offset;
}

static void
monitor_entry(void)
{
	unsigned *esp = (unsigned*)__get_cpu_var(gsptr);
	btmod_vcpu *v = (btmod_vcpu*)__get_cpu_var(vptr);
	vcpu_t *vcpu = &v->vcpu;
	void *tc_ptr;

	/* Reentrency */
	if (!(lock_owner == v->vcpu_id)) {
	  vcpu->eip = (void*)__get_cpu_var(tx_target);
	  vcpu->prev_tb = esp[8];
    tc_ptr = tb_gen_translated_code(v);
#ifdef PROFILING
    __get_cpu_var(shadowregs[4])++;
#endif
  } else {
		dprintk("FATAL: Reentrency detected!!!\n");
		tc_ptr = (void*)__get_cpu_var(tx_target);
	}
  __get_cpu_var(tx_target) = (unsigned long)tc_ptr;
	barrier();
  restore_guest_esp();
	restore_guest();
	jump_to_tc();
	NOT_REACHED();
}

void
init_callout_label(vcpu_t *vcpu)
{
	int a = 1;
	vcpu->callout_label = &&callout_label;
	if (a + 1 == 2) {
		return;
	}
callout_label:
	save_guest();
  save_guest_esp();
	setup_monitor_stack();
	monitor_entry();
}

void
patch_translated_eips(btmod_vcpu *v)
{
  struct task_struct *p, *t;
  uint32_t *esp, *ebp;
  uint32_t stack;
  vcpu_t *vcpu = &v->vcpu;

  do_each_thread(t, p) {
    if (p == get_current() || p->state == TASK_RUNNING) {
      continue;
    }
    esp = (uint32_t*)p->thread.sp;
    stack = (uint32_t)esp & ~(THREAD_SIZE-1);
    ebp = (uint32_t*)*esp;
    while ((uint32_t)ebp > stack) {
      vcpu->eip = (void*)ebp[1];
      if (vcpu->eip) {
        void *tc_ptr = tb_gen_translated_code(v);
        if (tc_ptr) {
          ebp[1] = (uint32_t)tc_ptr;
        }
        ebp = (uint32_t*)*ebp;
      }
    }
  } while_each_thread(t, p);
}

static inline bool
is_edge_cache_addr(btmod *bt, uint32_t eip)
{
  return addr_to_pool(&bt->info, eip) == POOL_EDGE;
}

static inline bool
insn_is_push_es(uint8_t *ptr)
{
  return *ptr == 0x06;
}

uint32_t
patch_until_fs_seen(btmod_vcpu *v, uint32_t cur_eip)
{
  btmod *bt = v->bt;
	vcpu_t *vcpu = &v->vcpu;
  uint8_t *code;
  insn_t insn;
  char const *opc;
  int len;
	uint32_t new_eip;
  uint32_t eip, prev_tb;

  do {
    code = (uint8_t*)cur_eip;
		if (insn_is_push_es(code)) {
			return 0;
		}

    len = disas_insn(v, code, cur_eip, &insn, 4, true);
    opc = opctable_name(bt, insn.opc);

    if (strstart(opc, "j", NULL) && insn.op[0].type == op_imm) {
      new_eip = operand_get_value(&insn.op[0]);

			if (is_edge_cache_addr(bt, new_eip)) {
				if (fetch_eip_and_prev_tb(v, new_eip, &eip, &prev_tb)) {
				  vcpu->eip = (void*)eip;
				  vcpu->prev_tb = prev_tb;
			    new_eip = (uint32_t)tb_gen_translated_code(v);
				} else {
					break;
				}
			}

      patch_until_fs_seen(v, new_eip);
			if (strstart(opc, "jmp", NULL)) {
			  break;
			}
		} else if (insn_is_terminating(bt, &insn)) {
			break;
		}
    cur_eip += len;
  } while (1);
  return 0;
}

static void *
tb_gen_translated_code_idt(btmod_vcpu *v)
{
	vcpu_t *vcpu = &v->vcpu;
	uint32_t tc_ptr;
	vcpu->prev_tb = 0;
	tc_ptr = (uint32_t)tb_gen_translated_code(v);
	patch_until_fs_seen(v, tc_ptr);
	return (void*)tc_ptr;
}

static void 
bt_store_idt(struct desc_ptr *dtr)
{
  asm volatile("sidt %0":"=m" (*dtr));
}

static void *
bt_read_idt_addr(gate_desc *idt, unsigned gate)
{
  return (void*)((idt[gate].a & 0xffff)
	  | (idt[gate].b & 0xffff0000));
}

static void
bt_put_idt_addr(gate_desc *idt, unsigned gate, void *addr)
{
  idt[gate].a = (idt[gate].a & 0xffff0000) | ((unsigned)addr & 0xffff);
	idt[gate].b = (idt[gate].b & 0x0000ffff) | ((unsigned)addr & 0xffff0000);
}

void
create_shadow_idt(btmod_vcpu *v)
{
	vcpu_t *vcpu = &v->vcpu;
	int i;
  void *idt_table;
  uint32_t lo;

	bt_store_idt(&native_idt_descr);
  idt_table = (void*)(native_idt_descr.address);
  memcpy(shadow.table, idt_table, sizeof(shadow));
 
  rdmsrl(MSR_IA32_SYSENTER_EIP, native_sysenter);
  lo = native_sysenter & 0xffffffff;
  vcpu->eip = (void*)lo;

  if (vcpu->eip) {
    target_sysenter = (uint32_t)tb_gen_translated_code_idt(v);
    ASSERT(target_sysenter);
  }

	for (i = 0; i < 256; i++) {
		vcpu->eip = bt_read_idt_addr(shadow.table, i);
		if (vcpu->eip && (i < 20 || i > 31) && i != 2) {
	    void *tc_ptr = tb_gen_translated_code_idt(v);
			ASSERT(tc_ptr);
			bt_put_idt_addr(shadow.table, i, tc_ptr);
		}
	}

	shadow_idt_descr.size = (NR_VECTORS * 16) - 1;
	shadow_idt_descr.address = (unsigned long)shadow.table;
}
