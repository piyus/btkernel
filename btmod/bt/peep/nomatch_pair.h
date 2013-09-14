#ifndef PEEP_NOMATCH_PAIRS_H
#define PEEP_NOMATCH_PAIRS_H
#include <linux/types.h>
#include "peep/insntypes.h"

typedef struct nomatch_pair_t {
  char reg1, reg2;
  tag_t tag1, tag2;
} nomatch_pair_t;

int nomatch_pair_add(nomatch_pair_t *nomatch_pairs, int max_nomatch_pairs,
    int num_nomatch_pairs, int reg1, tag_t tag1, int reg2, tag_t tag2);
size_t nomatch_pair2str(struct nomatch_pair_t const *nomatch_pair, char *buf,
    size_t buf_size);
size_t str2nomatch_pair(char const *buf, struct nomatch_pair_t *nomatch_pair);

#endif
