#include <types.h>
#include "peep/nomatch_pair.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include "peep/insntypes.h"
#include "peep/insn.h"
#include <debug.h>
#include "hypercall.h"

size_t
nomatch_pair2str(nomatch_pair_t const *nomatch_pair, char *buf,
    size_t buf_size)
{
  char reg1[32], reg2[32];
  operand_t op;
  op.type = op_reg;
  op.val.reg = nomatch_pair->reg1;
  op.tag.reg = nomatch_pair->tag1;
  op.size = 4;
  op.rex_used = 0;
  operand2str(&op, reg1, sizeof reg1);
  op.type = op_reg;
  op.val.reg = nomatch_pair->reg2;
  op.tag.reg = nomatch_pair->tag2;
  op.size = 4;
  op.rex_used = 0;
  operand2str(&op, reg2, sizeof reg2);

  return snprintf(buf, buf_size, "{%s,%s}", reg1, reg2);
}

size_t
str2nomatch_pair(char const *buf, nomatch_pair_t *nomatch_pair)
{
  operand_t op;
  int ret, nchars;
  bool b;
  char reg1[32], reg2[32];
  //printf("buf=%s\n", buf);
  ret = sscanf(buf, "{%[^,],%[^}]}%n", reg1, reg2, &nchars);
  ASSERT(ret >= 2);

  //printf("reg1=%s, reg2=%s\n", reg1, reg2);
  b = str2operand(&op, reg1);
  ASSERT(b);
  ASSERT(op.type == op_reg);
  ASSERT(op.size == 4);
  nomatch_pair->reg1 = op.val.reg;
  nomatch_pair->tag1 = op.tag.reg;

  b = str2operand(&op, reg2);
  ASSERT(b);
  ASSERT(op.type == op_reg);
  ASSERT(op.size == 4);
  nomatch_pair->reg2 = op.val.reg;
  nomatch_pair->tag2 = op.tag.reg;
  return nchars;
}

int
nomatch_pair_add(nomatch_pair_t *nomatch_pairs, int max_nomatch_pairs,
    int num_nomatch_pairs, int reg1, tag_t tag1, int reg2, tag_t tag2)
{
  int i;
  for (i = 0; i < num_nomatch_pairs; i++) {
    if (   nomatch_pairs[i].reg1 == reg1 && nomatch_pairs[i].tag1 == tag1
        && nomatch_pairs[i].reg2 == reg2 && nomatch_pairs[i].tag2 == tag2) {
      return num_nomatch_pairs;
    }
    if (   nomatch_pairs[i].reg2 == reg1 && nomatch_pairs[i].tag2 == tag1
        && nomatch_pairs[i].reg1 == reg2 && nomatch_pairs[i].tag1 == tag2) {
      return num_nomatch_pairs;
    }
  }
  /*
  printf("num_nomatch_pairs=%d, max_nomatch_pairs=%d\n", num_nomatch_pairs,
      max_nomatch_pairs);
      */
  ASSERT(num_nomatch_pairs < max_nomatch_pairs);
  nomatch_pairs[num_nomatch_pairs].reg1 = reg1;
  nomatch_pairs[num_nomatch_pairs].tag1 = tag1;
  nomatch_pairs[num_nomatch_pairs].reg2 = reg2;
  nomatch_pairs[num_nomatch_pairs].tag2 = tag2;
  num_nomatch_pairs++;
  return num_nomatch_pairs;
}
