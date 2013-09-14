#ifndef PEEP_PEEPTAB_H
#define PEEP_PEEPTAB_H

#define MAX_IN_LEN 1
#define MAX_OUT_LEN 10
#define MAX_CONSTANTS 24
#define MAX_NOMATCH_PAIRS 32

#include <lib/hash.h>
#include "peep/insntypes.h"
#include "peep/cpu_constraints.h"
#include "peep/nomatch_pair.h"

typedef struct peep_entry_t
{
  int n_tmpl;
  insn_t tmpl[MAX_IN_LEN];           /* input template.  */
  int label;
  unsigned n_temporaries;
  tag_t temporaries[MAX_TEMPORARIES];
  cpu_constraints_t cpu_constraints;
  int num_nomatch_pairs;
  struct nomatch_pair_t nomatch_pairs[MAX_NOMATCH_PAIRS];

  char const *name;                 /* for debugging purposes. */

  /* hash_elem for peep_tab. */
  struct hash_elem peeptab_elem;
} peep_entry_t;

#endif
