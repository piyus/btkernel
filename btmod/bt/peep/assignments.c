#include <types.h>
#include "peep/assignments.h"
#include <linux/types.h>
#include <linux/string.h>
#include <debug.h>
#include "peep/insntypes.h"
#include "peep/insn.h"
#include "peepgen.h"
//#include "sys/bootsector.h"
#include "sys/vcpu.h"
#ifndef __PEEPGEN__
#include <linux/slab.h>
#include "hypercall.h"
#endif

#ifndef EOF
#define EOF -1
#endif

/* Helper functions. */
static bool read_assignment(char const *line, assignments_t *assignments);


void
assignments_init(assignments_t *assignments)
{
  assignments->num_assignments = 0;
}

bool
assignments_getval(assignments_t const *assignments, operand_t const *templ,
    operand_t *out)
{
  int i;
  ASSERT(templ->tag.all != tag_const);
  for (i = 0; i < assignments->num_assignments; i++) {
    if (operands_equal(&assignments->arr[i].var, templ)) {
      memcpy(out, &assignments->arr[i].val, sizeof *out);
      return true;
    } else if (templ->type == op_reg && assignments->arr[i].var.type ==op_reg) {
      ASSERT(templ->tag.reg != tag_const);
      if (templ->val.reg == assignments->arr[i].var.val.reg) {
        ASSERT(templ->size < assignments->arr[i].var.size);
        memcpy(out, &assignments->arr[i].val, sizeof *out);
        out->size = templ->size;
        return true;
      }
    }
  }
  return false;
}

void
assignments_add(assignments_t *assignments, operand_t const *templ,
    operand_t const *op)
{
  ASSERT(assignments->num_assignments < MAX_VARIABLES);

  memcpy(&assignments->arr[assignments->num_assignments].var, templ,
      sizeof *templ);
  memcpy(&assignments->arr[assignments->num_assignments].val, op, sizeof *op);

  if (templ->type == op_reg && templ->size != 4) {
    /* For registers, add whole register assignments. */
    ASSERT(assignments->arr[assignments->num_assignments].var.size <= 4);
    ASSERT(assignments->arr[assignments->num_assignments].val.size <= 4);
    assignments->arr[assignments->num_assignments].var.size = 4;
    assignments->arr[assignments->num_assignments].val.size = 4;
  }

  assignments->num_assignments++;
}

void
append_assignments(assignments_t *assignments,
    assignments_t const *new_assignments)
{
  int i;

  ASSERT(assignments->num_assignments + new_assignments->num_assignments
      < MAX_VARIABLES);
  for (i = 0; i < new_assignments->num_assignments; i++) {
    memcpy(&assignments->arr[assignments->num_assignments+ i].var,
        &new_assignments->arr[i].var, sizeof (operand_t));
    memcpy(&assignments->arr[assignments->num_assignments + i].val,
        &new_assignments->arr[i].val, sizeof (operand_t));
  }
  assignments->num_assignments += new_assignments->num_assignments;
}

bool
variables_assign_random_regs(assignments_t *assignments, operand_t const *vars,
    int num_vars, operand_t const *used_consts, int num_used_consts)
{
  char used_regs[NUM_REGS];
  int i, r;

  //first generate used_regs
  memset(used_regs, 0x0, sizeof used_regs);
  for (i = 0; i < NUM_REGS; i++) {
    int j;
    for (j = 0; j < num_used_consts; j++) {
      if (used_consts[j].type == op_reg) {
        ASSERT(used_consts[j].tag.reg == tag_const);
        used_regs[i] = 1;
      }
    }
  }
  
  for (i = 0; i < num_vars; i++) {
    switch(vars[i].type) {
      case op_reg:
        ASSERT(vars[i].tag.reg != tag_const);
        for (r = NUM_REGS-1; r >= 0; r--) {
          if (!used_regs[r]) {
            operand_t regcons;

            regcons.type = op_reg;
            regcons.size = vars[i].size;
            regcons.rex_used = vars[i].rex_used;
            regcons.val.reg = r;
            regcons.tag.reg = tag_const;
            if (assignment_is_coherent(&vars[i], &regcons)) {
              assignments_add(assignments, &vars[i], &regcons);
              used_regs[r] = 1;
              break;
            }
          }
        }
        if (r < 0) {
          return false;
        }
        break;
      default:
        break;
    }
  }
  return true;
}


bool
variables_assign_random_segs(assignments_t *assignments, operand_t const *vars,
    int num_vars, operand_t const *used_consts, int num_used_consts)
{
  int const num_segs = 5;
  char used_segs[num_segs];
  int i, s;

  //first generate used_segs
  memset(used_segs, 0x0, sizeof used_segs);
  for (i = 0; i < num_segs; i++) {
    int j;
    for (j = 0; j < num_used_consts; j++) {
      if (used_consts[j].type == op_reg) {
        ASSERT(used_consts[j].tag.reg == tag_const);
        used_segs[i] = 1;
      }
    }
  }
  
  for (i = 0; i < num_vars; i++) {
    switch(vars[i].type) {
      case op_seg:
        ASSERT(vars[i].tag.seg != tag_const);
        for (s = 0; s < num_segs; s++) {
          if (!used_segs[s]) {
            operand_t segcons;

            segcons.type = op_seg;
            segcons.size = 0;                 ASSERT(vars[i].size == 0);
            segcons.val.seg = s;
            segcons.tag.seg = tag_const;
            assignments_add(assignments, &vars[i], &segcons);
            used_segs[s] = 1;
            break;
          }
        }
        if (s == num_segs) {
          return false;
        }
        break;
      default:
        break;
    }
  }
  return true;
}

void
variables_assign_random_consts(assignments_t *assignments,
    operand_t const *vars, int num_vars, operand_t const *used_consts,
    int num_used_consts)
{
  operand_t imm_cons;
  int i;

  for (i = 0; i < num_vars; i++) {
    switch(vars[i].type) {
      case op_imm:
        ASSERT(vars[i].tag.imm != tag_const);
        imm_cons.type = op_imm;
        imm_cons.size = vars[i].size;
        imm_cons.rex_used = vars[i].rex_used;
        imm_cons.val.imm = VAR_CONST(vars[i].val.imm);
        imm_cons.tag.imm = tag_const;
        assignments_add(assignments, &vars[i], &imm_cons);
        break;
      default:
        break;
    }
  }
}

void
variables_assign_random_prefix(assignments_t *assignments,
    operand_t const *vars, int num_vars)
{
  operand_t prefix_cons;
  int i;

  for (i = 0; i < num_vars; i++) {
    switch(vars[i].type) {
      case op_prefix:
        ASSERT(vars[i].tag.prefix != tag_const);
        prefix_cons.type = op_prefix;
        prefix_cons.size = vars[i].size;
        prefix_cons.rex_used = vars[i].rex_used;
        prefix_cons.val.prefix = 0;
        prefix_cons.tag.prefix = tag_const;
        assignments_add(assignments, &vars[i], &prefix_cons);
        break;
      default:
        break;
    }
  }
}


int
assignments_get_text_replacements(assignments_t const *assignments,
    char **vars, char **vals)
{
#define MAX_PATREP_SIZE 128
  int i;
  int num_vars = 0;
  for (i = 0; i < assignments->num_assignments; i++) {

    vars[num_vars] = (char*)kmalloc(MAX_PATREP_SIZE * sizeof(char), GFP_KERNEL);
    vals[num_vars] = (char*)kmalloc(MAX_PATREP_SIZE * sizeof(char), GFP_KERNEL);
    ASSERT(vars && vals);
    operand2str(&assignments->arr[i].var, vars[num_vars], MAX_OPERAND_STRSIZE);
    operand2str(&assignments->arr[i].val, vals[num_vars], MAX_OPERAND_STRSIZE);

    ASSERT(assignments->arr[i].var.tag.all != tag_const);
    ASSERT(assignments->arr[i].val.tag.all == tag_const);
    num_vars++;
  }

  /* For register operands, emit extra patterns. */
  for (i = 0; i < assignments->num_assignments; i++) {
    if (assignments->arr[i].var.type == op_reg) {
      operand_t var, val;
      char const dsizes[] = {2, 1};
      size_t s;

      ASSERT(assignments->arr[i].val.type == op_reg);
      ASSERT(assignments->arr[i].var.size == 4);
      ASSERT(assignments->arr[i].val.size == 4);

      memcpy(&var, &assignments->arr[i].var, sizeof(operand_t));
      memcpy(&val, &assignments->arr[i].val, sizeof(operand_t));

      for (s = 0; s < sizeof dsizes/sizeof dsizes[0]; s++) {
        vars[num_vars] = (char*)kmalloc(MAX_PATREP_SIZE * sizeof(char), GFP_KERNEL);
        vals[num_vars] = (char*)kmalloc(MAX_PATREP_SIZE * sizeof(char), GFP_KERNEL);
        ASSERT(vars && vals);
        var.size = dsizes[s];
        val.size = dsizes[s];
        operand2str(&var, vars[num_vars], MAX_OPERAND_STRSIZE);
        operand2str(&val, vals[num_vars], MAX_OPERAND_STRSIZE);
        num_vars++;
      }
    }
  }
  return num_vars;
}

int
assignments2str(assignments_t const *assignments, char *buf, int buf_size)
{
  char *ptr = buf, *end = buf + buf_size;
  int i;

  for (i = 0; i < assignments->num_assignments; i++) {
    ptr += operand2str(&assignments->arr[i].var, ptr, end - ptr);
    ptr += snprintf(ptr, end - ptr, " [%s] := ",
        tag2str(assignments->arr[i].var.tag.all));
    ptr += operand2str(&assignments->arr[i].val, ptr, end - ptr);
    ASSERT(ptr < end);
    *ptr++ = '\n';
  }
  *ptr++ = '\0';
  return ptr - buf;
}

bool
str2assignments(char const *buf, assignments_t *assignments)
{
  char line[80];
  char const *ptr = buf;
  int numchars;
  assignments_init(assignments);
  while (sscanf(ptr, "%[^\n]\n%n", line, &numchars) != EOF) {
    if (!read_assignment(line, assignments)) {
      return false;
    }
    ptr += numchars;
  }
  return true;
}

#define ESP_REG_NUM 4
static bool
val_satisfies_tag(int val, tag_t tag)
{
  switch (tag) {
    case tag_var: return true;
    case tag_eax: return (val == R_EAX);
    case tag_abcd: return (val >= 0 && val <= 3);
    case tag_no_esp: return (val != R_ESP);
    case tag_no_eax: return (val != R_EAX);
    case tag_no_eax_esp: return (val != R_EAX && val != R_ESP);
    case tag_no_cs_gs: return (val != R_GS && val != R_CS);
    case tag_cs_gs: return (val == R_GS || val == R_CS);
    default: NOT_REACHED();
  }
  return false;
}

bool
assignment_is_coherent(operand_t const *var, operand_t const *val)
{
  ASSERT(var->type == val->type);
  switch(var->type) {
    case op_reg:
      ASSERT(var->tag.reg != tag_const);
      ASSERT(val->tag.reg == tag_const);
      return val_satisfies_tag(val->val.reg, var->tag.reg);
    case op_seg:
      ASSERT(var->tag.seg != tag_const);
      ASSERT(val->tag.seg == tag_const);
      return val_satisfies_tag(val->val.seg, var->tag.seg);
    default:
      return true;
  }
  ASSERT(0);
}

bool
assignments_are_coherent(assignments_t const *assignments)
{
  int i;
  for (i = 0; i < assignments->num_assignments; i++) {
    if (!assignment_is_coherent(&assignments->arr[i].var,
          &assignments->arr[i].val)) {
      return false;
    }
  }
  return true;
}

void
assignments_copy(assignments_t *dst, assignments_t const *src)
{
  int i;

  dst->num_assignments = src->num_assignments;
  for (i = 0; i < dst->num_assignments; i++) {
    memcpy(&dst->arr[i].var, &src->arr[i].var, sizeof(operand_t));
    memcpy(&dst->arr[i].val, &src->arr[i].val, sizeof(operand_t));
  }
}

static bool
read_assignment(char const *line, assignments_t *assignments)
{
  char const *assign, *tag, *tag_end;
  int line_len = strlen(line), assign_off, tag_off;

  if (!(assign = strstr(line, ":="))) {
    ERR("String ':=' not found in assignment statement '%s'\n", line);
    return false;
  }
  assign_off = assign - line;

  {
    char variable[assign_off + 1];
    int constant_len = line_len - (assign_off + 2) + 1;
    char constant[constant_len];
    operand_t varop, consop;

    memcpy(variable, line, assign_off);
    variable[assign_off] = '\0';

    if (!(tag = strchr(variable, '['))) {
      ERR("Character '[' not found in variable name '%s'\n", variable);
      return false;
    }
    tag_off = tag - variable;
    {
      char varname[tag_off + 1];
      memcpy(varname, variable, tag_off);
      varname[tag_off] = '\0';

      if (!(tag_end = strchr(tag + 1, ']'))) {
        ERR("Character ']' not found in variable name '%s'\n", variable);
        return false;
      }
      {
        char tagname[tag_end - tag];
        memcpy(tagname, tag + 1, tag_end - (tag + 1));
        tagname[tag_end - (tag + 1)] = '\0';

        memcpy(constant, assign+2, constant_len);
        constant[constant_len] = '\0';

        if (!str2operand(&varop, varname)) {
          ERR("Could not convert variable '%s' to a valid operand.\n", varname);
          return false;
        }
        varop.tag.all = str2tag(tagname);
        if (!str2operand(&consop, constant)) {
          ERR("Could not convert constant '%s' to a valid operand.\n", constant);
          return false;
        }
        if (consop.type == op_imm && consop.val.imm == 0) {
          ERR("Immediate variable has value 0. Indicates parse error.\n");
          return false;
        }
        assignments_add(assignments, &varop, &consop);
      }
    }
  }
  return true;
}

int
assignments_get_regs(int *reg_indices, assignments_t const *assignments)
{
  int i, num_regs = 0;
  for (i = 0; i < assignments->num_assignments; i++) {
    if (assignments->arr[i].var.type == op_reg) {
      ASSERT(assignments->arr[i].val.type == op_reg);
      ASSERT(assignments->arr[i].var.tag.reg != tag_const);
      ASSERT(assignments->arr[i].val.tag.reg == tag_const);
      reg_indices[num_regs++] = i;
    }
  }
  return num_regs;
}

int
assignments_get_segs(int *seg_indices, assignments_t const *assignments)
{
  int i, num_segs = 0;
  for (i = 0; i < assignments->num_assignments; i++) {
    if (assignments->arr[i].var.type == op_seg) {
      ASSERT(assignments->arr[i].val.type == op_seg);
      ASSERT(assignments->arr[i].var.tag.reg != tag_const);
      ASSERT(assignments->arr[i].val.tag.reg == tag_const);
      seg_indices[num_segs++] = i;
    }
  }
  return num_segs;
}

bool
var_is_temporary(operand_t const *var)
{
  if (var->type == op_reg
      && var->tag.reg != tag_const
      && var->val.reg >= TEMP_REG0) {
    return true;
  }
  return false;
}

int
assignments_get_temporaries(assignments_t const *assignments,
    tag_t *temporaries)
{
  int i, ret = 0;
  for (i = 0; i < assignments->num_assignments; i++) {
    if (var_is_temporary(&assignments->arr[i].var)) {
      if (temporaries) {
        temporaries[ret] = assignments->arr[i].var.tag.reg;
      }
      ASSERT(ret < MAX_TEMPORARIES);
      ret++;
    }
  }
  return ret;
}
