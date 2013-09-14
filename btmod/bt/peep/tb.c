#include "peep/tb.h"
#include <debug.h>
#include <types.h>
#include <linux/string.h>
#include "peep/jumptable1.h"
#include "peep/jumptable2.h"
#include "sys/vcpu.h"
#include "peep/i386-dis.h"
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <asm/bug.h>
#include <asm/page.h>
#include "bt_vcpu.h"
#include "hypercall.h"
#include "mem/malloc.h"

#define TB 2
#define KERROR(__size) safe_printk("%s %d size=%d\n", __func__, __LINE__, __size);
#define TB_ALLOC(__bt, __size) tb_kmalloc(__bt, __size, POOL_KMALLOC, __func__, __LINE__)
#define TC_ALLOC(__bt, __size) tb_kmalloc(__bt, __size, POOL_TC, __func__, __LINE__)
#define EDGE_ALLOC(__bt, __size) tb_kmalloc(__bt, __size, POOL_EDGE, __func__, __LINE__)

#define NUM_BIN_CHARS 33
#define NUM_AS_CHARS 30

static bool
tc_less(struct rbtree_elem const *_a, struct rbtree_elem const *_b, void *aux)
{
  struct tb_t *a = rbtree_entry(_a, struct tb_t, tc_elem);
  struct tb_t *b = rbtree_entry(_b, struct tb_t, tc_elem);
  if ((a->tc_ptr + a->tc_boundaries[a->num_insns]) <= b->tc_ptr) {
    return true;
  }
  return false;
}

void
tb_init(struct rbtree *rb)
{
  rbtree_init(rb, &tc_less, NULL);
}

tb_t *
tb_find(struct rbtree *rb, const void *tc_ptr)
{
  struct tb_t tb;
  uint16_t tc_boundaries[1] = {1};
  struct rbtree_elem *found;
  struct tb_t *ret;

  tb.num_insns = 0;
  tb.tc_boundaries = tc_boundaries;
  tb.tc_ptr = (void *)tc_ptr;
  if (!(found = rbtree_find(rb, &tb.tc_elem))) {
    return NULL;
  }
  ASSERT(   rbtree_next(found) == rbtree_end(rb)
         || tc_less(found, rbtree_next(found), NULL));
  ret = rbtree_entry(found, struct tb_t, tc_elem);
  return ret;
}

void
tb_add(struct rbtree *rb, tb_t *tb)
{
  ASSERT(!rbtree_find(rb, &tb->tc_elem));
  rbtree_insert(rb, &tb->tc_elem);
}

target_ulong
tb_tc_ptr_to_eip_virt(struct rbtree *rb, uint32_t tc_ptr)
{
  size_t i;
  tb_t *tb;
  uint32_t tcptr;

  tb = tb_find(rb, (const void*)tc_ptr);
  if (tb == NULL) {
    dprintk("Addr Not Found %x\n", tc_ptr);
    return 0;
  }
  tcptr = (uint32_t)tb->tc_ptr;

  ASSERT(  tc_ptr >= tcptr 
     && tc_ptr < tcptr + tb->tc_boundaries[tb->num_insns]);

  for (i = 0; i < tb->num_insns; i++) {
    if (  tc_ptr >= tcptr + tb->tc_boundaries[i] 
       && tc_ptr < tcptr + tb->tc_boundaries[i + 1]) {
      return (i == 0)?tb->eip_virt:tb->eip_virt + tb->eip_boundaries[i-1];
    }
  }
  NOT_REACHED();
  return 0;
}

static void
tb_print_in_asm(btmod_vcpu *v, tb_t const *tb, unsigned size)
{
  unsigned n;
  char *ptr;
	unsigned long cur_addr = tb->eip_virt;
  tbprintk("IN:\n");
  
  for (n = 0; n < tb->num_insns; n++) {
    size_t dlen, num_bin_chars = NUM_BIN_CHARS, num_as_chars = NUM_AS_CHARS;
    char str[128];
    unsigned i;

    tbprintk("%lx:", cur_addr);
		ptr = (char*)cur_addr;
    dlen = sprint_insn(&v->bti386, (unsigned long)ptr, str, sizeof str, size, true);
    for (i = 0; i < dlen; i++) {
      tbprintk(" %02hhx", ptr[i]);
    }
    for (i = dlen*3; i < num_bin_chars; i++) {
      tbprintk(" ");
    }
    tbprintk(": %s", str);
    cur_addr += dlen;
    for (i = strlen(str) + 2; i < num_as_chars; i++) {
      tbprintk(" ");
    }
    tbprintk("\n");
  }
}


#if 0
static void
print_asm(btmod_vcpu *v, uint8_t *buf, size_t len, unsigned size)
{
	uint8_t *ptr = buf;
#define NUM_BIN_CHARS 33
#define NUM_AS_CHARS 30
	while (ptr - buf < (int)len) {
		char str[128];
		size_t dlen, num_bin_chars = NUM_BIN_CHARS, num_as_chars = NUM_AS_CHARS;
		unsigned i;

		dlen = sprint_insn(&v->bti386, (unsigned long)ptr, str, sizeof str, size, false);
		tbprintk("%p:", ptr);
		for (i = 0; i < dlen; i++) {
			tbprintk(" %02hhx", ptr[i]);
		}
		for (i = dlen*3; i < num_bin_chars; i++) {
			tbprintk(" ");
		}
		tbprintk(": %s", str);
		for (i = strlen(str) + 2; i < num_as_chars; i++) {
			tbprintk(" ");
		}
		tbprintk("\n");
		ptr += dlen;
	}
}
#endif


static void
tb_print_out_asm(btmod_vcpu *v, tb_t const *tb)
{
  char *disas_ptr = tb->tc_ptr;
  size_t const  num_bin_chars = NUM_BIN_CHARS;
  unsigned target_size = 4, i;     // on target, we always use protected mode.

  tbprintk("OUT:\n");
  while (disas_ptr - (char *)tb->tc_ptr < tb->code_len) {
    char str[128];
    size_t dlen;
    str[0] = '\0';

    dlen = sprint_insn(&v->bti386, (unsigned long)disas_ptr, str, sizeof str, target_size, false);
    tbprintk("%p:", disas_ptr);
    for (i = 0; i < dlen; i++) {
      tbprintk(" %02hhx", disas_ptr[i]);
    }
    ASSERT(dlen <= num_bin_chars);
    for (i = dlen*3; i < num_bin_chars; i++) {
      tbprintk(" ");
    }
    tbprintk(": %s\n", str);
    disas_ptr += dlen;
  }
  disas_ptr = tb->edge_ptr;
  while (disas_ptr - (char *)tb->edge_ptr < tb->edge_len) {
    char str[128];
    size_t dlen;
    str[0] = '\0';

    dlen = sprint_insn(&v->bti386, (unsigned long)disas_ptr, str, sizeof str, target_size, false);
    tbprintk("%p:", disas_ptr);
    for (i = 0; i < dlen; i++) {
      tbprintk(" %02hhx", disas_ptr[i]);
    }
    ASSERT(dlen <= num_bin_chars);
    for (i = dlen*3; i < num_bin_chars; i++) {
      tbprintk(" ");
    }
    tbprintk(": %s\n", str);
    disas_ptr += dlen;
  }
}

static void *
tb_kmalloc(btmod *bt, size_t size, unsigned pool, const char *func, int line)
{
  void *ret;
	if (size == 0) {
		return NULL;
	}
  ret = malloc_from_pool(&bt->info, pool, size);
  if (!ret) {
    BUG();
  }
  return ret;
}

#define KERROR(__size) safe_printk("%s %d size=%d\n", __func__, __LINE__, __size);

tb_t *
alloc_a_tb(btmod *bt)
{
  int alignment;
  void *alloc;
  tb_t *tb;
  alloc = TB_ALLOC(bt, sizeof *tb + 3);
  tb = (void *)(((unsigned long)alloc + 3) & ~3);
  ASSERT(((unsigned long)tb & 3) == 0);
  alignment = (unsigned long)tb - (unsigned long)alloc;
  ASSERT(alignment >= 0 && alignment < 4);
  tb->alignment = alignment;
  return tb;
}

void
tb_malloc(btmod *bt, tb_t *tb)
{
	tb->num_jumps = 0;
	tb->tc_ptr = TC_ALLOC(bt, tb->code_len);
	tb->edge_ptr = EDGE_ALLOC(bt, tb->edge_len);
  tb->tc_boundaries = TB_ALLOC(bt, (tb->num_insns + 1) * sizeof(*tb->tc_boundaries));
  tb->eip_boundaries = TB_ALLOC(bt, tb->num_insns * sizeof(*tb->eip_boundaries));
}

uint32_t
eip_virt_to_tc_ptr(tb_t *tb, uint32_t eip_virt)
{
  int i;
  int n_insn = tb->num_insns;
  target_ulong eip = tb->eip_virt;
  ASSERT(  eip_virt > eip
    && eip_virt < eip + tb->eip_boundaries[n_insn - 1]);

  for (i = 0; i < n_insn; i++) {
    if (eip_virt < eip + tb->eip_boundaries[i]) {
      target_ulong prev_eip = (i == 0)?eip:eip + tb->eip_boundaries[i-1];
      return (target_ulong)tb->tc_ptr
       + tb->tc_boundaries[i] + (eip_virt - prev_eip);
    }
  }
  return 0;
}

static bool
search_offset(tb_t *tb, uint16_t off)
{
  int i;
	for (i = 0; i < tb->num_jumps; i++) {
		if (tb->jump_boundaries[i] == off) {
			return true;
		}
	}
	return false;
}


/*
 * FIXME: Make TB_ALLOC to support free also
 */

void
tb_add_jump(btmod *bt, tb_t *tb, uint16_t off)
{
	if (!search_offset(tb, off)) {
	  uint16_t *jmp_arr;
		size_t size;
		size = (tb->num_jumps + 1) * sizeof(uint16_t);
	  jmp_arr = TB_ALLOC(bt, size);
    ASSERT(jmp_arr);
		size -= sizeof(uint16_t);
		memcpy((void*)jmp_arr, (void*)tb->jump_boundaries, size);
	  //free(tb->jump_boundaries);
    jmp_arr[tb->num_jumps] = off;
    tb->num_jumps++;
    tb->jump_boundaries = jmp_arr;
	}
}

void
print_asm_info(btmod_vcpu *v, tb_t *tb)
{
  if (loglevel & VCPU_LOG_TRANSLATE) {
    spin_lock(&print_lock);
    tbprintk("TB: eip_virt=%x idx=%x\n",
        tb->eip_virt, v->vcpu_id);
    tb_print_in_asm(v, tb, 4);
    tb_print_out_asm(v, tb);
    spin_unlock(&print_lock);
  }
}
