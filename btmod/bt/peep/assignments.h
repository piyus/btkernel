#ifndef PEEP_ASSIGNMENTS_H
#define PEEP_ASSIGNMENTS_H

#include <linux/stddef.h>
#include "peep/insntypes.h"

#define MAX_VARIABLES 8
#define TEMP_REG0 8

typedef struct assignments_t {
  struct {
    operand_t var;
    operand_t val;
  } arr[MAX_VARIABLES];
  int num_assignments;
} assignments_t;


void assignments_init(assignments_t *assignments);
bool assignments_getval(assignments_t const *assignments,
    struct operand_t const *templ, struct operand_t *out);
void assignments_add(assignments_t *assignments, struct operand_t const *templ,
    struct operand_t const *op);
void append_assignments(assignments_t *assignments,
    assignments_t const *new_assignments);
bool variables_assign_random_regs(assignments_t *assignments,
    operand_t const *vars, int num_vars, operand_t const *used_consts,
    int num_used_consts);
bool variables_assign_random_segs(assignments_t *assignments,
    operand_t const *vars, int num_vars, operand_t const *used_consts,
    int num_used_consts);
void variables_assign_random_consts(assignments_t *assignments,
    operand_t const *vars, int num_vars, operand_t const *used_consts,
    int num_used_consts);
void variables_assign_random_prefix(assignments_t *assignments,
    operand_t const *vars, int num_vars);
void assignments_copy(assignments_t *dst, assignments_t const *src);

int assignments_get_regs(int *reg_indices, assignments_t const *assignments);
int assignments_get_segs(int *seg_indices, assignments_t const *assignments);
int assignments_get_text_replacements(assignments_t const *assignments,
    char *vars[16], char *vals[16]);
bool assignments_are_coherent(assignments_t const *assignments);
bool assignment_is_coherent(operand_t const *var, operand_t const *val);

bool var_is_temporary(operand_t const *var);
int assignments_get_temporaries(assignments_t const *assignments,
    tag_t *temporaries);


int assignments2str(assignments_t const *assignments, char *buf, int buf_size);
bool str2assignments(char const *buf, assignments_t *assignments);

extern char const *regs8[], *regs16[], *regs32[];
extern char const *vregs8[], *vregs16[], *vregs32[];
extern char const *tregs8[], *tregs16[], *tregs32[];
extern size_t const num_regs, num_vregs, num_tregs;
extern char const *segs[];
extern char const *vsegs[];
extern size_t const num_segs, num_vsegs;

#endif /* peep/assignments.h */
