#include <types.h>
#include "peep/opctable.h"
#include <debug.h>
#include "sys/vcpu.h"
#ifndef __PEEPGEN__
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#endif
#include "hypercall.h"
#include "bt_vcpu.h"

static int find_sizeflag_num(btmod *bt, int sizeflag);

void
opctable_init(btmod *bt)
{
  int i;
	bt->ot.dpnum = 0;
  bt->ot.nametable_size = 0;
  bt->ot.sizeflagtable_size = 0;
  for (i = 0; i < MAX_DISAS_ENTRIES; i++) {
    int j;
    for (j = 0; j < MAX_SIZEFLAGS; j++) {
      bt->ot.opctable[i][j] = opc_inval;
    }
  }
  snprintf(bt->ot.nametable[bt->ot.nametable_size], sizeof bt->ot.nametable[bt->ot.nametable_size],"(bad)");
  bt->ot.nametable_size++;
}

void
opctable_insert(btmod *bt, char const *name, int dp_num, int sizeflag)
{
  unsigned i;
  opc_t opc = (opc_t)bt->ot.nametable_size;
  int sizeflag_num;

  for (i = 0; i < bt->ot.nametable_size; i++) {
    if (!strcmp(bt->ot.nametable[i], name)) {
      opc = i;
    }
  }
  ASSERT(opc < MAX_OPCS);
  sizeflag_num = find_sizeflag_num(bt, sizeflag);
  ASSERT(sizeflag_num < MAX_SIZEFLAGS);
  ASSERT(dp_num < MAX_DISAS_ENTRIES);

  bt->ot.opctable[dp_num][sizeflag_num] = opc;

  if (opc == (int)bt->ot.nametable_size) {
    snprintf(bt->ot.nametable[opc], sizeof bt->ot.nametable[opc], "%s", name);
    bt->ot.nametable_size++;
  }
  if (sizeflag_num == bt->ot.sizeflagtable_size) {
    bt->ot.sizeflagtable[sizeflag_num] = sizeflag;
    bt->ot.sizeflagtable_size++;
  }
}

void
opctable_print(btmod *bt)
{
  int dp_num;
  for (dp_num = 0; dp_num < MAX_DISAS_ENTRIES; dp_num++) {
    int sizeflag;
    for (sizeflag = 0; sizeflag < MAX_SIZEFLAGS; sizeflag++) {
      opc_t opc;
      opc = bt->ot.opctable[dp_num][sizeflag];
      dprintk("(%#x,%d)->%#x->%s\n", dp_num, sizeflag, opc,
          bt->ot.nametable[opc]);
    }
  }
}

opc_t
opctable_find(btmod *bt, int dp_num, int sizeflag)
{
  int sizeflagnum = find_sizeflag_num(bt, sizeflag);
  ASSERT(sizeflagnum < bt->ot.sizeflagtable_size);
  if (bt->ot.opctable[dp_num][sizeflagnum] == opc_inval) {
    return -1;
  }
  return bt->ot.opctable[dp_num][sizeflagnum];
}

char const *
opctable_name(btmod *bt, opc_t opc)
{
  return bt->ot.nametable[opc];
}

static int
find_sizeflag_num(btmod *bt, int sizeflag)
{
  int i;
  int sizeflag_num = bt->ot.sizeflagtable_size;
  for (i = 0; i < bt->ot.sizeflagtable_size; i++) {
    if (bt->ot.sizeflagtable[i] == sizeflag) {
      sizeflag_num = i;
      break;
    }
  }
  ASSERT(bt->ot.sizeflagtable_size <= MAX_SIZEFLAGS);
  ASSERT(sizeflag_num < MAX_SIZEFLAGS);
  return sizeflag_num;
}
