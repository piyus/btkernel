#ifndef PEEP_OPCTABLE_H
#define PEEP_OPCTABLE_H

struct btmod;
typedef int opc_t;
#define opc_inval 0

#define FGRPS_DPNUM(dp, rm)    
#define FLOAT_MEM_DPNUM(fp_index)

#define MAX_DISAS_ENTRIES 4192
#define MAX_SIZEFLAGS 4
#define MAX_OPCS 2048

typedef struct btmod_opctable {
	int dpnum;
  int sizeflagtable_size;
  size_t nametable_size;
  char nametable[MAX_OPCS][10];
  int sizeflagtable[MAX_SIZEFLAGS];
  opc_t opctable[MAX_DISAS_ENTRIES][MAX_SIZEFLAGS];
} btmod_opctable;

void opctable_init(struct btmod *bt);
void opctable_insert(struct btmod *bt, char const *name, int dp_num, int sizeflag);
opc_t opctable_find(struct btmod *bt, int dp_num, int sizeflag);
char const *opctable_name(struct btmod *bt, opc_t opc);
int opctable_size(struct btmod *bt);
void opctable_print(struct btmod *bt);

#endif
