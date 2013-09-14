#include <types.h>
#include "peep/regset.h"
#include <debug.h>
#include "peep/nomatch_pair.h"
#include "peep/assignments.h"
#ifndef __PEEPGEN__
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "hypercall.h"
#endif

void
regset_clear_all(regset_t *regset)
{
  memset(regset->cregs, 0, sizeof regset->cregs);
  memset(regset->vregs, 0, sizeof regset->vregs);
  memset(regset->tregs, 0, sizeof regset->tregs);
}

void
regset_mark(regset_t *regset, tag_t tag, int reg)
{
  if (tag == tag_const) {
      ASSERT(reg >= 0 && reg < NUM_REGS);
      regset->cregs[reg] = 1;
  } else {
    if (reg >= TEMP_REG0) {
      ASSERT(reg < TEMP_REG0 + MAX_TEMPORARIES);
      regset->tregs[reg - TEMP_REG0] = 1;
    } else {
      ASSERT(reg < NUM_REGS);
      regset->vregs[reg] = 1;
    }
  }
}

void
regset_clear(regset_t *regset, tag_t tag, int reg)
{
  if (tag == tag_const) {
      ASSERT(reg >= 0 && reg < NUM_REGS);
      regset->cregs[reg] = 0;
  } else {
    if (reg >= TEMP_REG0) {
      ASSERT(reg < TEMP_REG0 + MAX_TEMPORARIES);
      regset->tregs[reg - TEMP_REG0] = 0;
    } else {
      ASSERT(reg < NUM_REGS);
      regset->vregs[reg] = 0;
    }
  }
}


void
regset_diff(regset_t *dst, regset_t const *src)
{
  int i;
  for (i = 0; i < NUM_REGS; i++) {
    if (dst->cregs[i] && src->cregs[i]) {
      dst->cregs[i] = 0;
    }
  }
  for (i = 0; i < NUM_REGS; i++) {
    if (dst->vregs[i] && src->vregs[i]) {
      dst->vregs[i] = 0;
    }
  }
  for (i = 0; i < MAX_TEMPORARIES; i++) {
    if (dst->tregs[i] && src->tregs[i]) {
      dst->tregs[i] = 0;
    }
  }
}

void
regset_union(regset_t *dst, regset_t const *src)
{
  int i;
  for (i = 0; i < NUM_REGS; i++) {
    dst->cregs[i] |= src->cregs[i];
  }
  for (i = 0; i < NUM_REGS; i++) {
    dst->vregs[i] |= src->vregs[i];
  }
  for (i = 0; i < MAX_TEMPORARIES; i++) {
    dst->tregs[i] |= src->tregs[i];
  }
}

int
regset_output_nomatch_pairs(struct nomatch_pair_t *nomatch_pairs,
    int max_nomatch_pairs, int num_nomatch_pairs, regset_t const *idef,
    regset_t const *use)
{
  int i, n=num_nomatch_pairs;
  //dprintk("%s(): max_nomatch_pairs=%d\n", __func__, max_nomatch_pairs);
  for (i = 0; i < NUM_REGS; i++) {
    if (idef->vregs[i]) {
      int j;
      for (j = 0; j < NUM_REGS; j++) {
        if (use->cregs[j]) {
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n, i, 
              tag_var, j, tag_const);
        }
      }
      for (j = 0; j < NUM_REGS; j++) {
        if (i != j && use->vregs[j]) {
          //add nomatch pair i,j
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n, i, 
              tag_var, j, tag_var);
        }
      }
      for (j = 0; j < MAX_TEMPORARIES; j++) {
        if (use->tregs[j]) {
          //add nomatch pair i,TEMP_REG0+j
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n, i,
              tag_var, TEMP_REG0+j, tag_var);
        }
      }
    }
  }
  for (i = 0; i < MAX_TEMPORARIES; i++) {
    if (idef->tregs[i]) {
      int j;
      for (j = 0; j < NUM_REGS; j++) {
        if (use->cregs[j]) {
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n,
              TEMP_REG0 + i, tag_var, j, tag_const);
        }
      }
      for (j = 0; j < NUM_REGS; j++) {
        if (use->vregs[j]) {
          //add nomatch pair TEMP_REG0+i, j
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n,
              TEMP_REG0 + i, tag_var, j, tag_var);
        }
      }
      for (j = 0; j < MAX_TEMPORARIES; j++) {
        if (i != j && use->tregs[j]) {
          //add nomatch pair TEMP_REG0+i, TEMP_REG0+j
          n = nomatch_pair_add(nomatch_pairs, max_nomatch_pairs, n,
              TEMP_REG0 + i, tag_var, TEMP_REG0 + j, tag_var);
        }
      }
    }
  }
  return n;
}

int
regset_find_unused(regset_t const *regset)
{
  int i;
  for (i = 0; i < NUM_REGS; i++) {
    if (!regset->cregs[i]) {
      return i;
    }
  }
  NOT_REACHED();
	return 0;
}

void
regset2str(regset_t const *regset, char *buf, size_t buf_size)
{
  char *ptr = buf, *end = ptr + buf_size;
  int i;
  *ptr='\0';
  for (i = 0; i < NUM_REGS; i++) {
    if (regset->cregs[i]) {
      ptr += snprintf(ptr, end - ptr, "%s,", regs32[i]);
    }
  }
  for (i = 0; i < NUM_REGS; i++) {
    if (regset->vregs[i]) {
      ptr += snprintf(ptr, end - ptr, "%s,", vregs32[i]);
    }
  }
  for (i = 0; i < MAX_TEMPORARIES; i++) {
    if (regset->tregs[i]) {
      ptr += snprintf(ptr, end - ptr, "%s,", tregs32[i]);
    }
  }
}

void
str2regset(regset_t *regset, char *buf)
{
  NOT_IMPLEMENTED();
}
