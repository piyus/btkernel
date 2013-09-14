#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
//#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/init_task.h>
#include <linux/sort.h>
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
#include "btfix.h"
#include "config.h"

static struct exception_table_entry *__extable;
static int num_extable_entries = 0;
static bool extable_inited = false;

const struct exception_table_entry * 
__search_exception_tables(uint32_t addr)
{
#ifdef USER_EXTABLE
  int first = 0;
  int last = num_extable_entries - 1;

  while (first <= last) {
    int mid;
    mid = (first + last) / 2;
    if (__extable[mid].insn == addr) {
      return &__extable[mid];
    } else if (__extable[mid].insn < addr) {
      first = mid + 1;
    } else {
      last = mid - 1;
    }
  }
  return NULL;
#else
  return search_exception_tables(addr);
#endif
}

static int cmp_ex(const void *a, const void *b)
{ 
  const struct exception_table_entry *x = a, *y = b;
    
  if (x->insn > y->insn)
    return 1;
  if (x->insn < y->insn)
    return -1;
  return 0;
}

int
init_exception_table(void __user *table, int len)
{
  int size;
  num_extable_entries = len;
  if (num_extable_entries > MAX_EXTABLE_EIPS) {
    return 0;
  }
  size = sizeof(struct exception_table_entry) * len;
  __extable = (struct exception_table_entry*)btalloc(size, PAGE_KERNEL);
  if (__extable == NULL) {
    return 0;
  }
  if (copy_from_user((void*)__extable, table, size)) {
    return 0;
  }
  sort(__extable, num_extable_entries, sizeof(struct exception_table_entry), cmp_ex, NULL);
  extable_inited = true;
  return 1;
}

bool
extable_is_inited(void)
{
  return extable_inited;
}

bool
add_bt_exception_table(uint32_t eip, uint32_t fixup)
{
  struct module *mod = THIS_MODULE;
  struct exception_table_entry *extable;
  int addr;

  if (mod->num_exentries >= MAX_EXTABLE_EIPS) {
		//dprintk("All eips are already acquired\n");
    return false;
  }
  extable = (struct exception_table_entry *)mod->extable;
  addr = (int)&extable[mod->num_exentries].insn;
	
#ifndef OLDKERNEL
  extable[mod->num_exentries].insn = (int)eip - addr;
  extable[mod->num_exentries].fixup = fixup;
#else
  extable[mod->num_exentries].insn = eip;
  extable[mod->num_exentries].fixup = fixup;
#endif
  mod->num_exentries++;
  return true;
}

uint32_t
fetch_eip_and_prev_tb(btmod_vcpu *v, uint32_t cur_eip,
    uint32_t *eip, uint32_t *prev_tb)
{
  btmod *bt = v->bt;
  uint8_t *code;
  insn_t insn;
  char const *opc;
  int len;
  int next_insn_is_prev_tb = 0;

  do {
    code = (uint8_t*)cur_eip;
    len = disas_insn(v, code, cur_eip, &insn, 4, true);
    opc = opctable_name(bt, insn.opc);
    if (strstart(opc, "mov", NULL)) {
      *eip = operand_get_value(&insn.op[1]);
      next_insn_is_prev_tb = 1;
    } else if (next_insn_is_prev_tb) {
      *prev_tb = operand_get_value(&insn.op[0]);
      break;
    } else if (strstart(opc, "j", NULL)) {
      return 0;
    }
    cur_eip += len;
  } while (1);
  return 1;
}

static uint32_t
fetch_eip_from_edge(btmod_vcpu *v, uint32_t cur_eip)
{
  btmod *bt = v->bt;
  uint8_t *code;
  insn_t insn;
  operand_t *op1;
  char const *opc;
  int len, j;
  uint32_t ret_eip;// prev_tb;
  
  for (j = 0; j < 10; j++) {
    spin_lock(&bt->translation_lock);
    code = (uint8_t*)cur_eip;
    len = disas_insn(v, code, cur_eip, &insn, 4, true);
    opc = opctable_name(bt, insn.opc);
    op1 = &insn.op[0];
    if (strstart(opc, "jmp", NULL)) {
      uint32_t off = operand_get_value(op1);
      int pool;
      cur_eip = off;
      pool = addr_to_pool(&bt->info, cur_eip);
      if (pool == POOL_TC) {
        ret_eip = tb_tc_ptr_to_eip_virt(&bt->tc_tree, cur_eip);
        spin_unlock(&bt->translation_lock);
        return ret_eip;
      } /*else if (pool == POOL_EDGE) {
        fetch_eip_and_prev_tb(v, cur_eip, &ret_eip, &prev_tb);
        if (ret_eip) {
          spin_unlock(&bt->translation_lock);
          return ret_eip;
        }
      }*/
      len = 0;
    }
    cur_eip += len;
    spin_unlock(&bt->translation_lock);
  }
  return 0;
}

void
push_orig_eips(struct btmod *bt)
{
  struct task_struct *p, *t;
  uint32_t *esp, *ebp;
  uint32_t stack;
  uint32_t eip_virt;
  uint32_t ret_eip;
  int pool;

  do_each_thread(t, p) {
	  if (p == get_current()) {
      continue;
	  }
    esp = (uint32_t*)p->thread.sp;
    stack = (uint32_t)esp & ~(THREAD_SIZE-1);
    ebp = (uint32_t*)*esp;
    while ((uint32_t)ebp > stack) {
      ret_eip = ebp[1];
      pool = addr_to_pool(&bt->info, ret_eip);
      eip_virt = 0;
      switch (pool) {
        case POOL_TC: {
          eip_virt = tb_tc_ptr_to_eip_virt(&bt->tc_tree, ret_eip);
          break;
        }
        case POOL_EDGE: {
          eip_virt = fetch_eip_from_edge(bt->v, ret_eip);
          break;
        }
      }
      ASSERT(eip_virt);
      if (eip_virt) {
        ebp[1] = eip_virt;
      }
      ebp = (uint32_t*)*ebp;
    }
  } while_each_thread(t, p);
}
