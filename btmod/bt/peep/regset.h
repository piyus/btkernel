#ifndef PEEP_REGSET_H
#define PEEP_REGSET_H
#include <linux/stddef.h>
#include "peep/insntypes.h"

struct nomatch_pair_t;

typedef struct regset_t {
  char cregs[NUM_REGS];          /* actual registers. */
  char vregs[NUM_REGS];          /* register variables vr0d..vrNd. */
  char tregs[MAX_TEMPORARIES];   /* temporaries tr0d..trNd. */
} regset_t;

void regset_clear_all(regset_t *regset);
void regset_mark(regset_t *regset, tag_t tag, int reg);
void regset_clear(regset_t *regset, tag_t tag, int reg);
void regset_diff(regset_t *dst, regset_t const *src);
void regset_union(regset_t *dst, regset_t const *src);
int regset_output_nomatch_pairs(struct nomatch_pair_t *nomatch_pairs,
    int max_nomatch_pairs, int num_nomatch_pairs, regset_t const *idef,
    regset_t const *use);
int regset_find_unused(regset_t const *regset);

void regset2str(regset_t const *regset, char *buf, size_t buf_size);
void str2regset(regset_t *regset, char *buf);

#endif /* peep/regset.h */
