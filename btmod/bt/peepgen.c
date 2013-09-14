/* (c) Sorav Bansal. */
#include "peepgen.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "lib/macros.h"
#include "lib/utils.h"
#include "lib/hash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "debug.h"
#include "peep/peep.h"
#include "peep/insntypes.h"
#include "peep/insn.h"
#include "peep/peeptab.h"
#include "peep/assignments.h"
#include "peep/i386-dis.h"
#include "peep/opctable.h"
#include "peep/jumptable1.h"
#include "peep/cpu_constraints.h"
#include "peep/peeptab_defs.h"
#include "peep/regset.h"
#include "sys/vcpu.h"
#include "bt_vcpu.h"
#include "config.h"

#define PEEP_PREFIX_STR xstr(PEEP_PREFIX)
#define ROLLBACK_PREFIX_STR xstr(ROLLBACK_PREFIX)
#define ROLLBACK_PEEP_PREFIX_STR ROLLBACK_PREFIX_STR PEEP_PREFIX_STR
#define SNIPPET_PREFIX_STR    PEEP_PREFIX_STR "snippet_"

#define MODE16_STR "mode16_"

#define MAX_PEEP_SIZE 12

#define REGTYPE_REG 0
#define REGTYPE_SEG 1

#define ELF_CLASS ELFCLASS32
#define elf_check_arch(x) ( ((x) == EM_386) || ((x) == EM_486) )
#define EXE_RELOC ELF_RELOC
#include "peep/elf.h"

/* "Safe" macros for min() and max(). See [GCC-ext] 4.1 and 4.6. */
#define min(a,b)                                                      \
  ({typeof (a) _a = (a); typeof (b) _b = (b);                         \
    _a < _b ? _a : _b;                                                \
   })
#define max(a,b)                                                      \
  ({typeof (a) _a = (a); typeof(b) _b = (b);                          \
    _a > _b ? _a : _b;                                                \
   })

#define MAX_IN_REGS 8

btmod bt;
btmod_vcpu *v = NULL;

static char const *vregs[]={"vr0", "vr1", "vr2", "vr3", "vr4", "vr5", "vr6",
  "vr7"};
static char const *tregs[]={"tr0", "tr1", "tr2"};

/* functions not linked for peepgen. */
void debug_backtrace(void **frame_address)
{
  void **frame;
#define PRINT(...) do {printf(__VA_ARGS__);}while(0)

  PRINT("Call stack:");
  for (frame = frame_address;
      frame != NULL && frame[0] != NULL;
      frame = frame[0])
    PRINT(" %p", frame[1]);
  PRINT(".\n");
}

static void usage(void)
{
  printf("peepgen:\n"
         "usage: read_trans_tab [-o outfile] [-tg] trans.tab\n"
         "Generate peeptab.h and gencode.h from peep.tab\n"
         "-t    generate peeptab in peeptab.h\n"
         "-g    output gencode\n"
         "-f    output offsets.h\n"
      );
  exit(1);
}

static FILE *
xfopen(char const *filename, char const *mode)
{
  FILE *fp;
  if (!(fp = fopen(filename, mode))) {
    ERR("Can't open file '%s' : %s\n", filename, strerror(errno));
    exit(1);
  }
  return fp;
}



static void
find_regvars(char const **regvars, unsigned num_regvars,
    char const *assembly, operand_t *variables, int *num_variables)
{
  char const suffices[] = {'b', 'w', 'd'};
  unsigned i, s;

  for (i = 0; i < num_regvars; i++) {
    char chr;
    char const *ptr, *occurs;
    bool variable_exists = false;
    int j;
    operand_t op;
    char reg[16];
    
    snprintf(reg, sizeof reg, "%%%sd", regvars[i]); /*use d suffix (full reg)*/
    if (!str2operand(&op, reg)) {
      ERR("fatal: register variable name '%s' not recognized.\n", reg);
      exit(1);
    }
    snprintf(reg, sizeof reg, "%%%s", regvars[i]);
    assert(op.type == op_reg);
    assert(op.tag.reg != tag_const);

    for (j = 0; j < *num_variables; j++) {
      if (operands_equal(&variables[i], &op)) {
        variable_exists = true;
        break;
      }
    }
    if (variable_exists) {
      continue;
    }

    ptr = assembly;
    while ((occurs = strstr(ptr, regvars[i]))) {
      ptr = occurs + strlen(regvars[i]);
      chr = *ptr;
      for (s = 0; s < sizeof suffices/sizeof suffices[0]; s++) {
        if (chr == suffices[s]) {
          break;
        }
      }
      if (s == sizeof suffices/sizeof suffices[0]) {
        /* no valid suffix at the end. */
        continue;
      }
      assert(*num_variables < MAX_VARIABLES);
      memcpy(&variables[(*num_variables)++], &op, sizeof(operand_t));
      break;
    }
  }
}

static void
find_variables(char const *assembly, operand_t *variables, int *num_variables)
{
  char const **regs;
  unsigned i;

  /* registers. */
  for (regs = vregs; regs; regs = (regs == vregs)?tregs:NULL) {
    int num_regs = (regs == vregs)?sizeof vregs/sizeof vregs[0]:
      (regs == tregs)?sizeof tregs/sizeof tregs[0]:0;
    //int base_reg = (regs == vregs)?0:(regs == tregs)?TEMP_REG0:0;
    find_regvars(regs, num_regs, assembly, variables, num_variables);
  }

  /* immediates. */
  for (i = 0; i < MAX_IMM_VARS; i++) {
    operand_t imm_op;
    bool variable_exists = false;
    char imm[8];
    int j;

    snprintf(imm, sizeof imm, "C%d", i);
    if (!str2operand(&imm_op, imm)) {
      ASSERT(0);
    }
    ASSERT(imm_op.type == op_imm);
    for (j = 0; j < *num_variables; j++) {
      if (operands_equal(&variables[j], &imm_op)) {
        variable_exists = true;
        break;
      }
    }

    if (variable_exists) {
      continue;
    }

    if (strstr(assembly, imm)) {
      memcpy(&variables[*num_variables], &imm_op, sizeof(operand_t));
      (*num_variables)++;
    }
  }

  /* segments. */
  for (i = 0; i < num_vsegs; i++) {
    operand_t op;
    bool variable_exists = false;
    char seg[16];
    int j;

    snprintf(seg, sizeof seg, "%%%s", vsegs[i]);
    if (!str2operand(&op, seg)) {
      ERR("fatal: segment variable name '%s' not recognized.\n", seg);
      exit(1);
    }
    ASSERT(op.type == op_seg);
    ASSERT(op.tag.reg != tag_const);
    for (j = 0; j < *num_variables; j++) {
      if (operands_equal(&variables[j], &op)) {
        variable_exists = true;
        break;
      }
    }
    if (variable_exists) {
      continue;
    }

    if (strstr(assembly, (char *)seg+1)) {
      ASSERT(*num_variables < MAX_VARIABLES);
      memcpy(&variables[(*num_variables)++], &op, sizeof(operand_t));
    }
  }

  /* prefixes. */
  if (strstr(assembly, "prefix")) {
    operand_t op;
    op.type = op_prefix;
    op.size = 0;
    op.rex_used = 0;
    op.tag.prefix = tag_var;
    memcpy(&variables[(*num_variables)++], &op, sizeof(operand_t));
  }
}

static void
find_constants(char const *assembly, operand_t *constants, int *num_constants)
{
}

static void
read_in_text(FILE *fp, char *buf, int buf_len, int *linenum)
{
  char *ptr = buf, *end = buf + buf_len;
  int num_read;

  for (;;) {
    num_read = fscanf(fp, " %[^\n]\n", ptr);
    (*linenum)++;
    assert(num_read == 1);

    if (!strcmp(ptr, "--")) {
      *ptr = '\0';
      break;
    }

    ptr += strlen(ptr);
    *ptr++ = '\n';
  }
  if (ptr >= end) {
    printf("buf=%s\n", buf);
  }
  assert(ptr < end);
}

static cpu_constraints_t
cpu_constraints_read(char const *str)
{
  cpu_constraints_t ret;
  str2cpu_constraints(str, &ret);
  return ret;
}

static void
read_cpu_constraints(FILE *fp, char const *index_name,
    cpu_constraints_t *cpu_constraints)
{
  char line[80];
  char const *suffix;
  bool inside_entry = false;

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    if (line[0] == '\0') {
      fseek(fp, 1, SEEK_CUR);
    } else if (strstart(line, PEEP_PREFIX_STR, NULL)) {
      ASSERT(!inside_entry);
      if (strstart(line, index_name, &suffix) && suffix[0] == ':'
        && suffix[1] == '\0') {
        inside_entry = true;
      }
    } else if (inside_entry) {
      break;
    }
  }
  ASSERT(inside_entry);
  str2cpu_constraints(line, cpu_constraints);
}

#if 0
uint8_t
used_regs_read(char const *str)
{
  uint8_t ret;
  str2used_regs(str, &ret);
  return ret;
}

static void
read_used_regs(FILE *fp, char const *index_name, uint8_t *used_regs)
{
  char line[80];
  char const *suffix;
  bool inside_entry = false;

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    if (line[0] == '\0') {
      fseek(fp, 1, SEEK_CUR);
    } else if (strstart(line, PEEP_PREFIX_STR, NULL)) {
      ASSERT(!inside_entry);
      if (strstart(line, index_name, &suffix) && suffix[0] == ':'
        && suffix[1] == '\0') {
        inside_entry = true;
      }
    } else if (inside_entry) {
      break;
    }
  }
  ASSERT(inside_entry);
  str2used_regs(line, used_regs);
}
#endif

static int
read_nomatch_pairs(FILE *fp, char const *index_name,
    struct nomatch_pair_t *nomatch_pairs)
{
  char line[8192];
  char *ptr;
  char const *suffix;
  bool inside_entry = false;
  int num_nomatch_pairs = 0;

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    if (strstart(line, PEEP_PREFIX_STR, NULL)) {
      ASSERT(!inside_entry);
      if (strstart(line, index_name, &suffix) && suffix[0] == ':'
        && suffix[1] == '\0') {
        inside_entry = true;
      }
    } else if (inside_entry) {
      break;
    } else {
      ASSERT(0);
    }
  }
  ASSERT(inside_entry);
  ptr = line;
  if (!strcmp(ptr, "none")) {
    return 0;
  }
  do {
    ASSERT(num_nomatch_pairs < MAX_NOMATCH_PAIRS);
    ptr += str2nomatch_pair(ptr, &nomatch_pairs[num_nomatch_pairs]);
    num_nomatch_pairs++;
  } while(*ptr);
  return num_nomatch_pairs;
}

static void
read_constraint(operand_t *vars, int num_vars, cpu_constraints_t *constraints,
    /*uint8_t *used_regs, */char const *ptr)
{
  char *tmp, *colon, *variable, *range;
  char ptrcopy[strlen(ptr) + 1];
  operand_t varop, *var;
  int i;

  memcpy(ptrcopy, ptr, sizeof ptrcopy);

  colon = strchr(ptrcopy, ':');
  tmp = colon;
  if (!tmp) {
    ERR("No ':' in constraint '%s'\n", ptr);
    exit(1);
  }
  while (*--tmp == ' ');
  *++tmp = '\0';

  variable = ptrcopy;

  /* remove leading space in variable. */
  while (*variable++ == ' ');
  variable--;
  
  /* remove leading space in constraint. */
  range = colon + 1;
  while (*range++ == ' ');
  range--;

  /* remove trailing space in constraint. */
  tmp = range + strlen(range);
  while (*--tmp == ' ');
  *++tmp = '\0';

  if (!strcmp(variable, "cpu")) {
    *constraints = cpu_constraints_read(range);
  /*} else if (!strcmp(variable, "used_regs")) {
    *used_regs = used_regs_read(range);*/
  } else {
    char tag_range[64];
    if (!str2operand(&varop, variable)) {
      ERR("Could not read operand '%s'\n", variable);
      exit(1);
    }

    for (i = 0; i < num_vars; i++) {
      if (operands_equal(&vars[i], &varop)) {
        break;
      }
    }
    if (i == num_vars) {
      printf("%s not a used variable; yet defined in constraints.\n", variable);
      exit(0);
    }

    var = &vars[i];

    ASSERT(var->tag.reg == tag_var);
    snprintf(tag_range, sizeof tag_range, "tag_%s", range);
    var->tag.reg = str2tag(tag_range);
  }
}

static void
read_constraints(operand_t *vars, int num_vars,
    cpu_constraints_t *cpu_constraints, /*uint8_t *used_regs,*/ char const *ptr)
{
  char const *newline;
  *cpu_constraints = DEFAULT_CPU_CONSTRAINTS;
  //*used_regs = 0;

  while ((newline = strchr(ptr, '\n'))) {
    char line[newline - ptr + 1];
    memcpy(line, ptr, newline - ptr);
    line[newline - ptr] = '\0';
    read_constraint(vars, num_vars, cpu_constraints/*, used_regs*/, line);
    ptr = newline+1;
  }
}

static bool
read_debug_marker(FILE *fp, int *linenum)
{
  char string[16];
  off_t start = ftello(fp);
	int ret;

  ret = fscanf(fp, "%9[^\n]\n", string);
  if (!strcmp(string, "<debug>")) {
    (*linenum)++;
    return true;
  }
  fseeko(fp, start, SEEK_SET);
  return false;
}

static void
read_constraints_text(FILE *fp, char *buf, int buf_size, int *linenum)
{
  char *ptr = buf, *end = buf + buf_size;
  int num_read;
  int start_linenum = *linenum;
  off_t start = ftello(fp);

  for (;;) {
    num_read = fscanf(fp, " %[^\n]\n", ptr);
    (*linenum)++;
    assert(num_read == 1);

    if (!strcmp(ptr, "--")) {
      *ptr = '\0';
      break;
    } else if (!strcmp(ptr, "==")) {
      /* there was never a constraints section. rollback, and exit. */
      fseeko(fp, start, SEEK_SET);
      *linenum = start_linenum;
      *buf = '\0';
      break;
    }
    ptr += strlen(ptr);
    *ptr++ = '\n';
    assert(ptr < end);
  }
}

static void
read_out_text(FILE *fp, char *buf, int buf_len, int *linenum)
{
  char *ptr = buf, *end = buf + buf_len;
  int num_read;
  //int start_linenum = *linenum;
  //off_t start = ftello(fp);

  for (;;) {
    num_read = fscanf(fp, " %[^\n]\n", ptr);
    (*linenum)++;
    assert(num_read == 1);

    if (!strcmp(ptr, "==")) {
      *ptr = '\0';
      break;
    }
    ptr += strlen(ptr);
    *ptr++ = '\n';
  }
  assert(ptr < end);
}

static void
rename_as_code(char *out, int out_size, char const *in,
    assignments_t const *assignments, bool is_out)
{
  char augmented_text[strlen(in)+640];
  char renamed_text[2*(strlen(in) + 640)];
  int i, n_temporaries;
  char const *save_temporaries[] = {
    xstr(SAVE_TEMPORARY(0)),     xstr(SAVE_TEMPORARY(1)),
    xstr(SAVE_TEMPORARY(2)),     xstr(SAVE_TEMPORARY(3)),
    xstr(SAVE_TEMPORARY(4)),     xstr(SAVE_TEMPORARY(5)),
    xstr(SAVE_TEMPORARY(6)),     xstr(SAVE_TEMPORARY(7))
  };
  char const *restore_temporaries[] = {
    xstr(RESTORE_TEMPORARY(0)),     xstr(RESTORE_TEMPORARY(1)),
    xstr(RESTORE_TEMPORARY(2)),     xstr(RESTORE_TEMPORARY(3)),
    xstr(RESTORE_TEMPORARY(4)),     xstr(RESTORE_TEMPORARY(5)),
    xstr(RESTORE_TEMPORARY(6)),     xstr(RESTORE_TEMPORARY(7))
  };

  n_temporaries = assignments_get_temporaries(assignments, NULL);
  char *ptr = augmented_text, *end = ptr + sizeof augmented_text;
  char restore_temporaries_str[n_temporaries*80];
  char *rtptr = restore_temporaries_str;
  char *rtend = rtptr + sizeof restore_temporaries_str;
  char *patterns[1], *replacements[1];
  for (i = 0; i < n_temporaries; i++) {
    rtptr += snprintf(rtptr, rtend - rtptr, "%s; ", restore_temporaries[i]);
  }
  *rtptr = '\0';

  if (is_out) {
    for (i = 0; i < n_temporaries; i++) {
      ptr += snprintf(ptr, end - ptr, "%s\n", save_temporaries[i]);
    }
  }
  strncpy(ptr, in, end - ptr);
  ptr += strlen(in);

  if (is_out && !strstr(in, "no_restore_temporary")) {
    ptr += snprintf(ptr, end - ptr, "%s\n", restore_temporaries_str);
  }
  ASSERT(ptr < end);

  patterns[0] = "restore_temporaries";
  replacements[0] = restore_temporaries_str;
  make_string_replacements(renamed_text, sizeof renamed_text, augmented_text,
      patterns, replacements, 1);
  strncpy(augmented_text, renamed_text, sizeof augmented_text);

  char *vars[MAX_VARIABLES*4];
  char *vals[MAX_VARIABLES*4];
  int n_vars;
  ptr = out;
  end = out + out_size;
  n_vars = assignments_get_text_replacements(assignments, vars, vals);
  make_string_replacements(ptr, end - ptr, augmented_text, vars, vals, n_vars);

  for (i = 0; i < n_vars; i++) {
    free(vars[i]);
    free(vals[i]);
  }
}

static void
output_as_code(FILE *fp, char const *label, char const *text,
    cpu_mode_t cpu_mode)
{
  char const *remaining;
  fprintf(fp, ".global %s\n", label);
  fprintf(fp, "%s:\n", label);
  switch (cpu_mode) {
    case cpu_mode_16:
      fprintf(fp, ".code16\n");
      break;
    case cpu_mode_32:
      fprintf(fp, ".code32\n");
      break;
    case cpu_mode_any:
      if (   strstart(label, PEEP_PREFIX_STR, &remaining)
          && strstart(remaining, MODE16_STR, NULL)) {
        fprintf(fp, ".code16\n");
      } else {
        fprintf(fp, ".code32\n");
      }
      break;
    default:
      ASSERT(0);
  }
  fprintf(fp, "%s\n\n", text);
}

static void
output_assignments(FILE *fp, char const *label,assignments_t const *assignments)
{
  char buf[1024];
  
  assignments2str(assignments, buf, sizeof buf);
  fprintf(fp, "%s:\n", label);
  fprintf(fp, "%s\n", buf);
}

static void
output_cpu_constraints(FILE *fp, char const *label,
    cpu_constraints_t const *cpu_constraints)
{
  char buf[1024];

  cpu_constraints2str(cpu_constraints, buf, sizeof buf);
  fprintf(fp, "%s:\n", label);
  fprintf(fp, "%s\n", buf);
}

/*
static void
output_used_regs(FILE *fp, char const *label, uint8_t const *used_regs)
{
  char buf[1024];
  used_regs2str(used_regs, buf, sizeof buf);
  fprintf(fp, "%s:\n", label);
  fprintf(fp, "%s\n", buf);
}
*/

static void
rename_and_output_as_code(FILE *fp, char const *label, char const *text,
    assignments_t const *assignments, cpu_mode_t cpu_mode, bool is_out)
{
  char renamed_text[2*(strlen(text) + 640)+ 1];

  rename_as_code(renamed_text, sizeof renamed_text, text, assignments, is_out);
  output_as_code(fp, label, renamed_text, cpu_mode);
}

char rbcode_buf[65536*64];
char *rbcode_ptr = rbcode_buf;
char *rbcode_end = (char *)rbcode_buf + sizeof rbcode_buf;

static void
output_code_with_rollback(FILE *out_S, char const *label, char const *out_text,
    assignments_t const *assignments,
    cpu_mode_t mode)
{
  rename_and_output_as_code(out_S, label, out_text, assignments, mode, true);
}

static void
generate_output_code(FILE *out_S, char const *index_name,
    char const *out_text, assignments_t const *assignments)
{
  int i, num_regs, num_segs;
  int reg_indices[assignments->num_assignments];
  int seg_indices[assignments->num_assignments];
  char label[strlen(index_name) + 16];
  char renamed_out_text[strlen(out_text) + 64];
  assignments_t rank_assignments;

  assignments_init(&rank_assignments);
  assignments_copy(&rank_assignments, assignments);

  num_regs = assignments_get_regs(reg_indices, &rank_assignments);
  num_segs = assignments_get_segs(seg_indices, &rank_assignments);

  /* output the code for the root entry. */
  snprintf(label, sizeof label, "%s", index_name);
  output_code_with_rollback(out_S, label, out_text, assignments,
      cpu_mode_32);

  for (i = 0; i < num_regs; i++) {
    int r = reg_indices[i];
    int j;
    for (j = 0; j < NUM_REGS; j++) {
      int old_reg = rank_assignments.arr[r].val.val.reg;
      rank_assignments.arr[r].val.val.reg = j;

      if (assignment_is_coherent(&rank_assignments.arr[r].var,
            &rank_assignments.arr[r].val)) {
        char *patterns[1], *replacements[1];
        char pattern[256], replacement[256];
        patterns[0] = pattern; replacements[0] = replacement;
        snprintf(label, sizeof label, "%s_r_%d_%d", index_name,
            rank_assignments.arr[r].var.val.reg, j);
        snprintf(pattern, sizeof pattern, "%s", index_name);
        snprintf(replacement, sizeof replacement, "%s", label);
        make_string_replacements(renamed_out_text, sizeof renamed_out_text,
            out_text, patterns, replacements, 1);
        output_code_with_rollback(out_S, label, renamed_out_text,
            &rank_assignments, cpu_mode_32);
      }
      rank_assignments.arr[r].val.val.reg = old_reg;
    }
  }

  for (i = 0; i < num_segs; i++) {
    int s = seg_indices[i];
    int j;
    for (j = 0; j < NUM_SEGS; j++) {
      int old_seg = rank_assignments.arr[s].val.val.seg;
      rank_assignments.arr[s].val.val.seg = j;

      if (assignment_is_coherent(&rank_assignments.arr[s].var,
            &rank_assignments.arr[s].val)) {
        char *patterns[1], *replacements[1];
        char pattern[256], replacement[256];
        patterns[0] = pattern; replacements[0] = replacement;
        snprintf(label, sizeof label,  "%s_s_%d_%d", index_name,
            rank_assignments.arr[s].var.val.reg, j);
        snprintf(pattern, sizeof pattern, "%s", index_name);
        snprintf(replacement, sizeof replacement, "%s", label);
        make_string_replacements(renamed_out_text, sizeof renamed_out_text,
            out_text, patterns, replacements, 1);
        output_code_with_rollback(out_S, label, renamed_out_text,
            &rank_assignments, cpu_mode_32);
      }
      rank_assignments.arr[s].val.val.seg = old_seg;
    }
  }
}

#define MAX_PATTERNS 16
static void
gen_as_entry(char const *in_text, char const *out_text,
    char const *constraints_text, char const *index_name, FILE *in_S,
    FILE *out_S, FILE *vars_fp, FILE *constraints_fp/*, FILE *used_regs_fp*/)
{
  operand_t vars[MAX_VARIABLES];
  operand_t consts[100];
  int num_vars = 0, num_consts = 0;
  assignments_t assignments;
  cpu_constraints_t cpu_constraints;

  find_variables(in_text, vars, &num_vars);
  find_variables(out_text, vars, &num_vars);

  read_constraints(vars, num_vars, &cpu_constraints/*, &used_regs*/,
      constraints_text);

  find_constants(in_text, consts, &num_consts);
  find_constants(out_text, consts, &num_consts);

  assignments_init(&assignments);
  if (!variables_assign_random_regs(&assignments, vars, num_vars, consts,
        num_consts)) {
    ERR("Register constraints infeasible at %s.\n", index_name);
    exit(1);
  }
  variables_assign_random_segs(&assignments, vars, num_vars, consts,
      num_consts);

  /* Only those variables must be assigned before generating output code,
   * which do not emit relocations. */
  generate_output_code(out_S, index_name, out_text, &assignments);

  variables_assign_random_consts(&assignments, vars, num_vars, consts,
      num_consts);
  variables_assign_random_prefix(&assignments, vars, num_vars);

  rename_and_output_as_code(in_S, index_name, in_text, &assignments,
      cpu_mode_any, false);
  output_assignments(vars_fp, index_name, &assignments);
  output_cpu_constraints(constraints_fp, index_name, &cpu_constraints);
}

static int
gen_mem_modes32(char const *mem_modes[], char const *mem_constraints[],
    char const *in_text)
{
  static char lmem_modes[16][32], lmem_constraints[16][32];
  unsigned i;
  char vr0[8], vr1[8], c0[8];

  for (i = 0; i < sizeof vregs/sizeof vregs[0]; i++) {
    if (!strstr(in_text, vregs[i])) {
      strlcpy(vr0, vregs[i], sizeof vr0);
      break;
    }
  }
  for (i = 0; i < sizeof vregs/sizeof vregs[0]; i++) {
    if (strcmp(vr0, vregs[i]) && !strstr(in_text, vregs[i])) {
      strlcpy(vr1, vregs[i], sizeof vr1);
      break;
    }
  }
  for (i = 0; i < MAX_IMM_VARS; i++) {
    snprintf(c0, sizeof c0, "C%d", i);
    if (!strstr(in_text, c0)) {
      break;
    }
  }

  snprintf(lmem_modes[0], sizeof lmem_modes[0], "%s(%%%sd,%%eiz,1)", c0, vr0);
  snprintf(lmem_modes[1], sizeof lmem_modes[1], "%s(%%%sd,%%%sd,1)", c0, vr0,
      vr1);
  snprintf(lmem_modes[2], sizeof lmem_modes[2], "%s(%%%sd,%%%sd,2)", c0, vr0,
      vr1);
  snprintf(lmem_modes[3], sizeof lmem_modes[3], "%s(%%%sd,%%%sd,4)", c0, vr0,
      vr1);
  snprintf(lmem_modes[4], sizeof lmem_modes[4], "%s(%%%sd,%%%sd,8)", c0, vr0,
      vr1);
  snprintf(lmem_modes[5], sizeof lmem_modes[5], "%s(,%%%sd, 1)", c0, vr0);
  snprintf(lmem_modes[6], sizeof lmem_modes[6], "%s(,%%%sd, 2)", c0, vr0);
  snprintf(lmem_modes[7], sizeof lmem_modes[7], "%s(,%%%sd, 4)", c0, vr0);
  snprintf(lmem_modes[8], sizeof lmem_modes[8], "%s(,%%%sd, 8)", c0, vr0);
  snprintf(lmem_modes[9], sizeof lmem_modes[9], "%s(,%%eiz, 1)", c0);

  unsigned num_mem_modes = 10;
  for (i = 0; i < num_mem_modes; i++) {
    lmem_constraints[i][0] = '\0';  /* by default, no additional constraints. */

    if (i >= 1 && i <= 4) {
      /* An index register cannot take esp. */
      snprintf(lmem_constraints[i], sizeof lmem_constraints[i],
          "%%%sd : no_esp\n", vr1);
    }
    if (i >= 5 && i <= 8) {
      /* An index register cannot take esp. */
      snprintf(lmem_constraints[i], sizeof lmem_constraints[i],
          "%%%sd : no_esp\n", vr0);
    }
  }


  for (i = 0; i < num_mem_modes; i++) {
    mem_modes[i] = lmem_modes[i];
    mem_constraints[i] = lmem_constraints[i];
  }
  return num_mem_modes;
}

static int
gen_mem_modes16(char const *mem_modes[], char const *mem_constraints[],
    char const *in_text)
{
  static char lmem_modes[16][32], lmem_constraints[16][32];
  char c0[8];
  int i;

  for (i = 0; i < MAX_IMM_VARS; i++) {
    snprintf(c0, sizeof c0, "C%d", i);
    if (!strstr(in_text, c0)) {
      break;
    }
  }

  snprintf(lmem_modes[0], sizeof lmem_modes[0], "%s(%%bx,%%si)", c0);
  snprintf(lmem_modes[1], sizeof lmem_modes[1], "%s(%%bx,%%di)", c0);
  snprintf(lmem_modes[2], sizeof lmem_modes[2], "%s(%%bp,%%si)", c0);
  snprintf(lmem_modes[3], sizeof lmem_modes[3], "%s(%%bp,%%di)", c0);
  snprintf(lmem_modes[4], sizeof lmem_modes[4], "%s(%%si)", c0);
  snprintf(lmem_modes[5], sizeof lmem_modes[5], "%s(%%di)", c0);
  snprintf(lmem_modes[6], sizeof lmem_modes[6], "%s(%%bp)", c0);
  snprintf(lmem_modes[7], sizeof lmem_modes[7], "%s(%%bx)", c0);
  snprintf(lmem_modes[8], sizeof lmem_modes[8], "%s", c0);

  int num_mem_modes = 9;
  for (i = 0; i < num_mem_modes; i++) {
    lmem_constraints[i][0] = '\0';  /* by default, no additional constraints. */
  }

  for (i = 0; i < num_mem_modes; i++) {
    mem_modes[i] = lmem_modes[i];
    mem_constraints[i] = lmem_constraints[i];
  }
  return num_mem_modes;
}


static int
gen_mem_modes(char const *mem_modes[], char const *mem_constraints[],
    char const *in_text)
{
  if (strstr(in_text, "MEM16")) {
    return gen_mem_modes16(mem_modes, mem_constraints, in_text);
  } else if (strstr(in_text, "MEM32")) {
    return gen_mem_modes32(mem_modes, mem_constraints, in_text);
  }
  NOT_REACHED();
}

static void
append_constraints(char *all_constraints, int all_constraints_size,
    char const *constraints, char * const *new_constraints,
    int num_new_constraints)
{
  char *ptr = all_constraints, *end = all_constraints + all_constraints_size;
  char const *cptr;
  int i;

  cptr = constraints;
  while ((*ptr++ = *cptr++));
  ptr--;

  for (i = 0; i < num_new_constraints; i++) {
    if (new_constraints[i]) {
      cptr = new_constraints[i];
      while ((*ptr++ = *cptr++));
      ptr--;
    }
  }
  ASSERT(ptr < end);
}

#define EXCP_PATTERN "excp"
static void
gen_as_files(char const *filename)
{
  FILE *fp, *in_S, *out_S, *vars_fp, *constraints_fp/*, *used_regs_fp*/;

  int linenum = 0, entrynum = 0;
  static char line[4096];
  line[0] = '\0';

  fp = xfopen(filename, "r");
  in_S = xfopen("in.S", "w");
  out_S = xfopen("out.S", "w");
  vars_fp = xfopen("vars", "w");
  constraints_fp = xfopen("cpu_constraints", "w");

  fprintf(in_S, ".allow_index_reg\n");
  fprintf(out_S, ".allow_index_reg\n");
  fprintf(out_S, "#include \"peepgen_offsets.h\"\n");
  fprintf(out_S, "#include \"peep/peeptab_defs.h\"\n");

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    static char in_text[1024], out_text[16384], constraints_text[8192],
         all_constraints[4096];
    char renamed_in_text[sizeof in_text], renamed_out_text[sizeof out_text];
    char const *jcc_conds[32];
    char const *mem_modes[16], *mem_constraints[16];
    char *patterns[MAX_PATTERNS], *replacements[MAX_PATTERNS],
         *new_constraints[MAX_PATTERNS];
    int i, num_jcc_conds, num_mem_modes, num_patterns = 0;
    cpu_mode_t peeptab_entry_type = cpu_mode_any;
    char base_index_name[256];
    int debug_marker;
    bool is_snippet = false;
    char snippet_name[80];

    ASSERT(strlen(line) < sizeof line);
    linenum++;
    if (line[0] == '#' || is_whitespace(line)) {
      if (line[0] == '\0') {
        fseek(fp, 1, SEEK_CUR); //if nothing was read, advance file pointer
      }
      continue;
    } else if (!strcmp(line, "entry_16:")) {
      peeptab_entry_type = cpu_mode_16;
    } else if (!strcmp(line, "entry:")) {
      peeptab_entry_type = cpu_mode_32;
    } else if (strstart(line, SNIPPET_PREFIX_STR, NULL)) {
      int ret;
      ret = sscanf(line, "%[^:]:", snippet_name);
      ASSERT(ret == 1);
      is_snippet = true;
    } else {
      ERR("Parse error at '%s' line %d:'%s'\n", filename, linenum, line);
      exit(1);
    }
    entrynum++;

    debug_marker = read_debug_marker(fp, &linenum);

    if (!is_snippet) {
      snprintf(base_index_name, sizeof base_index_name,
          PEEP_PREFIX_STR "%sline%d%s",
          (peeptab_entry_type == cpu_mode_16)?MODE16_STR:"", linenum,
          debug_marker?"_debug":"");
    } else {
      snprintf(base_index_name, sizeof base_index_name, "%s", snippet_name);
    }

    read_in_text(fp, in_text, sizeof in_text, &linenum);
    read_constraints_text(fp,constraints_text,sizeof constraints_text,&linenum);
    read_out_text(fp, out_text, sizeof out_text, &linenum);
    linenum++;

    if (strstr(in_text, "MEM")) {
      num_mem_modes = gen_mem_modes(mem_modes, mem_constraints, in_text);
    } else {
      mem_modes[0] = "MEM";
      num_mem_modes = 1;
    }

    if (strstr(in_text, "jCC")) {
      jcc_conds[0] = "jne";    jcc_conds[1] = "je";
      jcc_conds[2] = "jg";    jcc_conds[3] = "jle";
      jcc_conds[4] = "jl";    jcc_conds[5] = "jge";
      jcc_conds[6] = "ja";    jcc_conds[7] = "jbe";
      jcc_conds[8] = "jb";    jcc_conds[9] = "jae";
      jcc_conds[10] = "jo";   jcc_conds[11] = "jno";
      jcc_conds[12] = "js";   jcc_conds[13] = "jns";
      jcc_conds[14] = "jp";   jcc_conds[15] = "jpe";
      num_jcc_conds = 16;
    } else {
      jcc_conds[0] = "jCC";    jcc_conds[1] = "jNCC";
      num_jcc_conds = 1;
    }


    for (i = 0; i < num_mem_modes; i++) {
      char mem_index_name[256];
      int j, p, old_num_patterns = num_patterns;
      assert(num_patterns == 0);
      char *optr;
      int num_excp_rollbacks;

      if (strcmp(mem_modes[i], "MEM")) {
        snprintf(mem_index_name, sizeof mem_index_name, "%s_mem%i",
            base_index_name, i);
        patterns[num_patterns] = (char *)malloc(8*sizeof(char));
        if (strstr(in_text, "MEM16")) {
          strlcpy(patterns[num_patterns], "MEM16", 8);
        } else if (strstr(in_text, "MEM32")) {
          strlcpy(patterns[num_patterns], "MEM32", 8);
        } else {
          NOT_REACHED();
        }
        replacements[num_patterns] = (char *)malloc(64*sizeof(char));
        strlcpy(replacements[num_patterns], mem_modes[i], 64);
        new_constraints[num_patterns] = (char *)malloc(64*sizeof(char));
        strlcpy(new_constraints[num_patterns], mem_constraints[i], 64);
        num_patterns++;
      } else {
        strncpy(mem_index_name, base_index_name, sizeof mem_index_name);
      }

      for (j = 0; j < num_jcc_conds; j++) {
        char jcc_index_name[256];
        int p, k;

        if (strcmp(jcc_conds[j], "jCC")) {
          snprintf(jcc_index_name, sizeof jcc_index_name, "%s_%s",
              mem_index_name, jcc_conds[j]);
          patterns[num_patterns] = (char *)malloc(8*sizeof(char));
          strlcpy(patterns[num_patterns], "jCC", 8);
          replacements[num_patterns] = (char *)malloc(8*sizeof(char));
          strlcpy(replacements[num_patterns], jcc_conds[j], 8);
          new_constraints[num_patterns] = NULL;
          num_patterns++;

          patterns[num_patterns] = (char *)malloc(8*sizeof(char));
          strlcpy(patterns[num_patterns], "jNCC", 8);
          replacements[num_patterns] = (char *)malloc(8*sizeof(char));
          strlcpy(replacements[num_patterns],
              (j%2)?jcc_conds[j-1]:jcc_conds[j+1],8);
          new_constraints[num_patterns] = NULL;
          num_patterns++;
        } else {
          strncpy(jcc_index_name, mem_index_name, sizeof jcc_index_name);
        }

        //rename exit labels
        for (k = 0; k < 3; k++) {
          patterns[num_patterns] = (char *)malloc(16*sizeof(char));
          snprintf(patterns[num_patterns], 16, ".edge%d", k);
          if (strstr(out_text, patterns[num_patterns])) {
            replacements[num_patterns] = (char *)malloc(64*sizeof(char));
            snprintf(replacements[num_patterns], 64, ".edge%d.%s", k,
                jcc_index_name);
            new_constraints[num_patterns] = NULL;
            num_patterns++;
          }
        }

        //rename exception_rollbacks
        patterns[num_patterns] = malloc(64*sizeof(char));
        snprintf(patterns[num_patterns], 64, EXCP_PATTERN "00");
        if (strstr(out_text, patterns[num_patterns])) {
          replacements[num_patterns] = malloc(256*sizeof(char));
          snprintf(replacements[num_patterns], 256, EXCP_PATTERN "00_%s",
              jcc_index_name);
          new_constraints[num_patterns] = NULL;
          num_patterns++;
        }

        make_string_replacements(renamed_in_text, sizeof renamed_in_text,
            in_text, patterns, replacements, num_patterns);
        make_string_replacements(renamed_out_text, sizeof renamed_out_text,
            out_text, patterns, replacements, num_patterns);
        append_constraints(all_constraints, sizeof all_constraints,
            constraints_text, new_constraints, num_patterns);

        //renumber exception_rollbacks in out_text
        num_excp_rollbacks = 0;
        optr = renamed_out_text;
        while ((optr = strstr(optr, EXCP_PATTERN "00")) != NULL) {
          char followchr = *(optr + strlen(EXCP_PATTERN "00"));
          optr += snprintf(optr, 256, EXCP_PATTERN "%02d",
              num_excp_rollbacks);
          *optr = followchr;
          num_excp_rollbacks++;
        }

        gen_as_entry(renamed_in_text, renamed_out_text, all_constraints,
            jcc_index_name, in_S, out_S, vars_fp, constraints_fp/*,
            used_regs_fp*/);

        for (p = old_num_patterns; p < num_patterns; p++) {
          free(patterns[p]);            patterns[p] = NULL;
          free(replacements[p]);        patterns[p] = NULL;
          if (new_constraints[p]) {
            free(new_constraints[p]);   new_constraints[p] = NULL;
          }
        }
        num_patterns = old_num_patterns;
      }

      for (p = old_num_patterns; p < num_patterns; p++) {
        free(patterns[p]);          patterns[p] = NULL;
        free(replacements[p]);      replacements[p] = NULL;
        if (new_constraints[p]) {
          free(new_constraints[p]); new_constraints[p] = NULL;
        }
      }
      num_patterns = old_num_patterns;
    }
    assert(num_patterns == 0);
  }
  fprintf(in_S, PEEP_PREFIX_STR "end:\n");
  fprintf(out_S, PEEP_PREFIX_STR "end:\n");
  fprintf(out_S, "%s", rbcode_buf);
  fprintf(out_S, ROLLBACK_PEEP_PREFIX_STR "end:\n");
  fclose(fp);

  fclose(in_S);
  fclose(out_S);
  fclose(vars_fp);
  fclose(constraints_fp);
}

//int dbg_level = 1;
void
debug_panic (const char *file, int line, const char *function,
    const char *message, ...) {
  va_list args;

  printf ("Error at %s:%d in %s(): ", file, line, function);

  va_start (args, message);
  vprintf (message, args);
  printf ("\n");
  va_end (args);
  debug_backtrace (__builtin_frame_address(0));
  exit(1);
}

static void
gen_offsets(void)
{
  FILE *fp = fopen("peepgen_offsets.h", "w");
  fprintf(fp, "#ifndef BUILD_OFFSETS_H\n");
  fprintf(fp, "#define BUILD_OFFSETS_H\n");
  fprintf(fp, "#define VCPU_EIP_OFF %d\n", offsetof(vcpu_t, eip));
  fprintf(fp, "#define VCPU_PREV_TB_OFF %d\n", offsetof(vcpu_t, prev_tb));
  fprintf(fp, "#define VCPU_EDGE_OFF %d\n", offsetof(vcpu_t, edge));
  fprintf(fp, "#define VCPU_CALLOUT_LABEL_OFF %d\n",
      offsetof(vcpu_t, callout_label));
  fprintf(fp, "#ifndef JUMPTABLE1_MASK\n"
              "#define JUMPTABLE1_MASK %#x\n"
              "#endif\n", JUMPTABLE1_MASK);
  fprintf(fp, "#endif\n");
  fclose(fp);
}

static void *
load_data(int fd, long offset, unsigned int size)
{
  char *data;

  data = malloc(size);
  if (!data) {
    ERR("malloc() failed.\n");
    return NULL;
  }
  lseek(fd, offset, SEEK_SET);
  if (read(fd, data, size) != (int)size) {
    ERR("fd read failed. errno=%s\n", strerror(errno));
    free(data);
    return NULL;
  }
  return data;
}

/* executable information */
#define EXE_SYM ElfW(Sym)
#define SHT_RELOC SHT_REL

EXE_SYM *symtab;
int nb_syms;
int text_shndx;
uint8_t *text;
EXE_RELOC *relocs;
int nb_relocs;

/* ELF file info */
struct elf_shdr *shdr;
uint8_t **sdata;
struct elfhdr ehdr;
char *strtab;

static struct elf_shdr *
find_elf_section(struct elf_shdr *shdr, int shnum,
    const char *shstr, const char *name)
{
  int i;
  const char *shname;
  struct elf_shdr *sec;

  for(i = 0; i < shnum; i++) {
    sec = &shdr[i];
    if (!sec->sh_name)
      continue;
    shname = shstr + sec->sh_name;
    if (!strcmp(shname, name))
      return sec;
  }
  return NULL;
}

static int
find_reloc(unsigned sh_index)
{
  struct elf_shdr *sec;
  unsigned i;

  for(i = 0; i < ehdr.e_shnum; i++) {
    sec = &shdr[i];
    if (sec->sh_type == SHT_RELOC && sec->sh_info == sh_index) 
      return i;
  }
  return 0;
}

static unsigned long get_rel_offset(EXE_RELOC *rel)
{
  return rel->r_offset;
}

static char *get_rel_sym_name(EXE_RELOC *rel)
{
  return strtab + symtab[ELFW(R_SYM)(rel->r_info)].st_name;
}

static char *get_sym_name(EXE_SYM *sym)
{
  return strtab + sym->st_name;
}

static int
symval_compare(const void *_a, const void *_b)
{
  EXE_SYM const *a = *((EXE_SYM **)_a), *b = *((EXE_SYM * *)_b);

  return (a->st_value - b->st_value);
}

/* load an elf object file */
static bool
load_object(const char *filename)
{
  int fd;
  struct elf_shdr *sec, *symtab_sec, *strtab_sec, *text_sec;
  int i;
  char *shstr;

  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ERR("can't open file '%s'", filename);
    return false;
  }

  /* Read ELF header.  */
  if (read(fd, &ehdr, sizeof (ehdr)) != sizeof (ehdr)) {
    ERR("unable to read file header");
    return false;
  }

  /* Check ELF identification.  */
  if (ehdr.e_ident[EI_MAG0] != ELFMAG0
      || ehdr.e_ident[EI_MAG1] != ELFMAG1
      || ehdr.e_ident[EI_MAG2] != ELFMAG2
      || ehdr.e_ident[EI_MAG3] != ELFMAG3
      || ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
    ERR("bad ELF header");
    return false;
  }

  if (ehdr.e_ident[EI_CLASS] != ELF_CLASS) {
    ERR("Unsupported ELF class");
    return false;
  }
  if (ehdr.e_type != ET_REL) {
    ERR("ELF object file expected");
    return false;
  }
  if (ehdr.e_version != EV_CURRENT) {
    ERR("Invalid ELF version");
    return false;
  }
  if (!elf_check_arch(ehdr.e_machine)) {
    ERR("Unsupported CPU (e_machine=%d)", ehdr.e_machine);
    return false;
  }

  /* read section headers */
  shdr = load_data(fd, ehdr.e_shoff, ehdr.e_shnum * sizeof(struct elf_shdr));

  /* read all section data */
  sdata = malloc(sizeof(void *) * ehdr.e_shnum);
  memset(sdata, 0, sizeof(void *) * ehdr.e_shnum);

  for(i = 0;i < ehdr.e_shnum; i++) {
    sec = &shdr[i];
    if (sec->sh_type != SHT_NOBITS) {
      sdata[i] = load_data(fd, sec->sh_offset, sec->sh_size);
    }
  }

  sec = &shdr[ehdr.e_shstrndx];
  shstr = (char *)sdata[ehdr.e_shstrndx];

  /* text section */

  text_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".text");
  if (!text_sec) {
    ERR("could not find .text section");
    return false;
  }
  text_shndx = text_sec - shdr;
  text = sdata[text_shndx];

  /* find text relocations, if any */
  relocs = NULL;
  nb_relocs = 0;
  i = find_reloc(text_shndx);
  if (i != 0) {
    relocs = (ELF_RELOC *)sdata[i];
    nb_relocs = shdr[i].sh_size / shdr[i].sh_entsize;
  }

  symtab_sec = find_elf_section(shdr, ehdr.e_shnum, shstr, ".symtab");
  if (!symtab_sec) {
    ERR("could not find .symtab section");
    return false;
  }
  strtab_sec = &shdr[symtab_sec->sh_link];

  symtab = (ElfW(Sym) *)sdata[symtab_sec - shdr];
  strtab = (char *)sdata[symtab_sec->sh_link];

  nb_syms = symtab_sec->sh_size / sizeof(ElfW(Sym));
  close(fd);
  return true;
}

static int
get_symtab_entries(EXE_SYM **entries, char const *prefix)
{
  int num_entries = 0;
  int i;
  EXE_SYM *sym;

  for (i = 0, sym = symtab; i < nb_syms; i++, sym++) {
    char const *name = get_sym_name(sym);
    if (strstart(name, prefix, NULL)) {
      entries[num_entries++] = sym;
    }
  }
  qsort(entries, num_entries, sizeof entries[0], symval_compare);
  return num_entries;
}

static void
read_assignments(FILE *fp, char const *index_name, assignments_t *assignments)
{
  char line[80];
  char buf[512];
  char *ptr = buf, *end = ptr + sizeof buf;
  char const *suffix;
  off_t bol = ftello(fp);       /* beginning of line. */
  bool inside_entry = false;
  bool success;

  //we assume that assignments are always read from beginning to end of the
  //file. If this assumption were to be relaxed, we would wrap-around the
  //file pointer on reaching EOF.

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    if (line[0] == '\0') {
      fseek(fp, 1, SEEK_CUR);
    } else if (strstart(line, PEEP_PREFIX_STR, NULL)) {
      if (inside_entry) {
        /* entry finished. */
        inside_entry = false;
        fseeko(fp, bol, SEEK_SET);
        break;
      }
      if (strstart(line, index_name, &suffix) && suffix[0] == ':'
        && suffix[1] == '\0') {
        inside_entry = true;
      }
    } else if (inside_entry) {
      ASSERT(end - ptr >= (int)strlen(line));
      memcpy(ptr, line, strlen(line));
      ptr += strlen(line);
      *ptr++ = '\n';
    }
    bol = ftello(fp);
  }
  *ptr++ = '\0';

  ASSERT(ptr < end);
  success = str2assignments(buf, assignments);
  ASSERT(success);
}

static int
disas_insns(insn_t *insns, size_t insns_size, uint8_t const *code,
    size_t codesize, int operand_size)
{
  uint8_t const *ptr = code;
  unsigned n_insns = 0;
  while (ptr - code < (int)codesize) {
    size_t len;
    /* In the first disas_insn() invocation, get the length.
       In the second invocation, use it to get the appropriate
       value for jump/call operands. */
    len = disas_insn(v, ptr, 0, &insns[n_insns], operand_size, false);
    len = disas_insn(v, ptr, -len, &insns[n_insns], operand_size, false);
    if (len == 0) {
      return 0;
    }
    ptr += len;
    n_insns++;
  }
  ASSERT(n_insns <= insns_size);
  return n_insns;
}

static void
insns_rename_constants(insn_t *insns, int n_insns,
    assignments_t const *assignments)
{
  int i;
  for (i = 0; i < n_insns; i++) {
    insn_rename_constants(&insns[i], assignments);
  }
}


#define gen_mem_ord_assn(assignments, oassignments, op, field, ftype) do {     \
  if (op->tag.mem.field != tag_const) {                                        \
    operand_t _op, val, val2;                                                  \
    _op.type = op_##ftype;                                                     \
    _op.size = (_op.type == op_seg)?0:4/*op->val.mem.addrsize*/;               \
    _op.val.ftype = op->val.mem.field;                                         \
    _op.tag.ftype = op->tag.mem.field;                                         \
    if (!assignments_getval(assignments, &_op, &val)) {                        \
      ASSERT(0);                                                               \
    }                                                                          \
    if (!assignments_getval(&oassignments, &_op, &val2)) {                     \
      assignments_add(&oassignments, &_op, &val);                              \
    }                                                                          \
  }                                                                            \
} while(0)

static void
get_assignments_without_temps(assignments_t *assignments_no_temps,
    assignments_t const *assignments)
{
  int i;
  assignments_init(assignments_no_temps);
  for (i = 0; i < assignments->num_assignments; i++) {
    if (!var_is_temporary(&assignments->arr[i].var)) {
      assignments_add(assignments_no_temps, &assignments->arr[i].var,
          &assignments->arr[i].val);
    }
  }
}

static void
gen_vars_ordered_for_entry(FILE *outfile, char const *name,
    uint8_t const *code, size_t codesize, assignments_t const *assignments)
{
  assignments_t ordered_assignments;
  insn_t insns[MAX_PEEP_SIZE];
  int i, n_insns, size;
  char const *remaining_name;
  assignments_t assignments_no_temps;

  ASSERT(!strstart(name, SNIPPET_PREFIX_STR, NULL));
  if (!strstart(name, PEEP_PREFIX_STR, &remaining_name)) {
    ASSERT(0);
  }
  if (strstart(remaining_name, MODE16_STR, NULL)) {
    size = 2;
  } else {
    size = 4;
  }

  get_assignments_without_temps(&assignments_no_temps, assignments);

  n_insns = disas_insns(insns, sizeof insns/sizeof insns[0], code, codesize,
      size);
  ASSERT(n_insns >0 && n_insns < MAX_PEEP_SIZE);

  insns_rename_constants(insns, n_insns, &assignments_no_temps);

  /* generate ordered variables. */
  assignments_init(&ordered_assignments);
  for (i = 0; i < n_insns; i++) {
    int j;
    for (j = 0; j < 3; j++) {
      operand_t const *op = &insns[i].op[j];
      operand_t val, val2;
			char varstr[64];
			operand2str(op, varstr, sizeof varstr);
      if (op->tag.all != tag_const) {
        ASSERT(op->tag.all != tag_const);
        ASSERT(op->type != op_mem);
        if (!assignments_getval(assignments, op, &val)) {
          ASSERT(0);
        }
        if (!assignments_getval(&ordered_assignments, op, &val2)) {
          assignments_add(&ordered_assignments, op, &val);
        }
      } else if (op->type == op_mem) {
        ASSERT(op->tag.mem.all == tag_const);
        if (op->val.mem.segtype == segtype_sel) {
          gen_mem_ord_assn(assignments, ordered_assignments, op, seg.sel, seg);
        } else {
          gen_mem_ord_assn(assignments, ordered_assignments, op, seg.desc, imm);
        }
        gen_mem_ord_assn(assignments, ordered_assignments, op, base, reg);
        gen_mem_ord_assn(assignments, ordered_assignments, op, index, reg);
        gen_mem_ord_assn(assignments, ordered_assignments, op, disp, imm);
      }
    }
  }
  for (i = 0; i < assignments->num_assignments; i++) {
    operand_t val;
    if (!assignments_getval(&ordered_assignments, &assignments->arr[i].var,
          &val)) {
      /* This must be a temporary. */
      if (!var_is_temporary(&assignments->arr[i].var)) {
        char varstr[64];
        operand2str(&assignments->arr[i].var, varstr, sizeof varstr);
        printf("%s: type=%d, tag.reg=%d, val.reg=%d\n", name,
            assignments->arr[i].var.type,
            assignments->arr[i].var.tag.reg,
            assignments->arr[i].var.val.reg);
        ERR("%s: Non-temporary variable %s not present in input, but present "
            "in output.\n", name, varstr);
        exit(1);
      }
      assignments_add(&ordered_assignments, &assignments->arr[i].var,
          &assignments->arr[i].val);
    }
  }
  output_assignments(outfile, name, &ordered_assignments);
}

static void
gen_vars_ordered(char const *outfilename)
{
  FILE *outfile, *vars_fp;
  int i;
  
  if (!load_object("in.o")) {
    exit(1);
  }
  vars_fp = xfopen("vars", "r");
  outfile = xfopen(outfilename, "w");
  EXE_SYM *entries[nb_syms];
  int num_entries;
  num_entries = get_symtab_entries(entries, PEEP_PREFIX_STR);

  for (i = 0; i < num_entries - 1; i++) {
    assignments_t assignments;
    EXE_SYM *sym = entries[i], *next_sym = entries[i + 1];
    char const *name = get_sym_name(sym);
    Elf32_Addr val = sym->st_value;
    Elf32_Word size = next_sym->st_value - sym->st_value;

    read_assignments(vars_fp, name, &assignments);

    if (strstart(name, SNIPPET_PREFIX_STR, NULL)) {
      /* any order is OK for snippets. The programmer needs
         to find out the order himself. */
      output_assignments(outfile, name, &assignments);
    } else {
      gen_vars_ordered_for_entry(outfile, name, &text[val], size, &assignments);
    }
  }
  fclose(vars_fp);
  fclose(outfile);
}

static void
gen_peeptab_entry(FILE *outfile, char const *index_name,
    uint8_t const *code, int codesize, assignments_t const *assignments,
    cpu_constraints_t const *cpu_constraints/*, uint8_t const *used_regs*/,
    struct nomatch_pair_t const *nomatch_pairs, int num_nomatch_pairs)
{
  char const *remaining_name;
  insn_t insns[MAX_PEEP_SIZE];
  int i, n_insns, n_temporaries = 0;
  tag_t temporaries[MAX_TEMPORARIES];
  assignments_t assignments_no_temps;
  unsigned size;

  if (!strstart(index_name, PEEP_PREFIX_STR, &remaining_name)) {
    ASSERT(0);
  }
  if (strstart(remaining_name, MODE16_STR, NULL)) {
    size = 2;
  } else {
    size = 4;
  }

  n_insns = disas_insns(insns, sizeof insns/sizeof insns[0], code, codesize,
      size);
  ASSERT(n_insns > 0 && n_insns < MAX_PEEP_SIZE);

  get_assignments_without_temps(&assignments_no_temps, assignments);
  insns_rename_constants(insns, n_insns, &assignments_no_temps);

  ASSERT(!strstart(index_name, SNIPPET_PREFIX_STR, NULL));
  n_temporaries = assignments_get_temporaries(assignments, temporaries);

  /* generate peeptab entry. */
  fprintf(outfile, "{%d,{", n_insns);
  for (i = 0; i < n_insns; i++) {
    insn_t const *insn = &insns[i];
    int j;
    fprintf(outfile, "{%d, \"%s\",{",insns[i].opc,opctable_name(&bt, insns[i].opc));
    for (j = 0; j < 3; j++) {
      operand_t const *op = &insn->op[j];
      if (op->type == invalid) {
        break;
      } else if (op->type == op_mem) {
        fprintf(outfile, "{op_%s,%d,%d,{.mem={%u,%u,{%u},%d,%u,%d,%llu}},"
            "{.mem={%s,{%s},%s,%s,%s}}},",
            get_opertype_str(op->type), op->size, op->rex_used,
            op->val.mem.addrsize, op->val.mem.segtype, op->val.mem.seg.all,
            op->val.mem.base, op->val.mem.scale, op->val.mem.index,
            op->val.mem.disp,
            tag2str(op->tag.mem.all), tag2str(op->tag.mem.seg.all),
            tag2str(op->tag.mem.base), tag2str(op->tag.mem.index),
            tag2str(op->tag.mem.disp));
      } else {
        uint64_t val;
        char format[128];
        char const *val_format = NULL;
        switch (op->type) {
          case op_reg: val = op->val.reg; break;
          case op_seg: val = op->val.seg; break;
          case op_imm: val = op->val.imm; val_format="%#llx"; break;
          case op_cr: val = op->val.cr; break;
          case op_db: val = op->val.db; break;
          case op_tr: val = op->val.tr; break;
          case op_mmx: val = op->val.mmx; break;
          case op_xmm: val = op->val.xmm; break;
          case op_3dnow: val = op->val.d3now; break;
          case op_prefix: val = op->val.prefix; break;
          default: ASSERT(0);
        }
        if (!val_format) {
          val_format="%llu";
        }
        snprintf(format, sizeof format, "%s%s%s", "{op_%s,%d,%d,{.%s=",
            val_format, "},{.%s=%s}},");
        fprintf(outfile, format, get_opertype_str(op->type), op->size,
            op->rex_used, get_opertype_str(op->type), val,
            get_opertype_str(op->type), tag2str(op->tag.all));
      }
    }
    fprintf(outfile, "}},");
  }
  fprintf(outfile, "},%s,%d,{", index_name, n_temporaries);
  for (i = 0; i < n_temporaries; i++) {
    fprintf(outfile, "%s,", tag2str(temporaries[i]));
  }
  fprintf(outfile, "},%#llx,%d,{", *cpu_constraints, num_nomatch_pairs);
  for (i = 0; i < num_nomatch_pairs; i++) {
    fprintf(outfile, "{%d,%d,%s,%s},", nomatch_pairs[i].reg1,
        nomatch_pairs[i].reg2, tag2str(nomatch_pairs[i].tag1),
        tag2str(nomatch_pairs[i].tag2));
  }
  fprintf(outfile, "},\"%s\",},\n", index_name);
}

static void
gen_defs_from_file(FILE *outfile, char const *filename, char const *prefix)
{
  int i, num_entries;

  if (!load_object(filename)) {
    exit(1);
  }

  EXE_SYM *entries[nb_syms];
  num_entries = get_symtab_entries(entries, PEEP_PREFIX_STR);

  for (i = 0; i < num_entries - 1; i++) {
    char const *name = get_sym_name(entries[i]);
    ASSERT(strstart(name, PEEP_PREFIX_STR, NULL));
    fprintf(outfile, "DEF(%s)\n", name+strlen(PEEP_PREFIX_STR));
  }
}

static void
gen_defs(char const *outfilename)
{
  FILE *outfile;
  if (!(outfile = fopen(outfilename, "w"))) {
    ERR("fopen('%s', \"w\") failed. %s\n", outfilename, strerror(errno));
    exit(1);
  }
  gen_defs_from_file(outfile, "in.o", PEEP_PREFIX_STR);
  fclose(outfile);
}

static void
gen_snippet_entry(FILE *snippet_file, char const *index_name,
    assignments_t const *assignments, nomatch_pair_t const *nomatch_pairs,
    int num_nomatch_pairs)
{
  int i;
  ASSERT(strstart(index_name, SNIPPET_PREFIX_STR, NULL));
  fprintf(snippet_file, "tag_t const %s_temporary_tags[] = {", index_name);
  for (i = 0; i < assignments->num_assignments; i++) {
    if (var_is_temporary(&assignments->arr[i].var)) {
      fprintf(snippet_file, "%s,", tag2str(assignments->arr[i].var.tag.reg));
    }
  }
  fprintf(snippet_file, "};\n");
  fprintf(snippet_file, "int %s_num_nomatch_pairs = %d;\n", index_name,
      num_nomatch_pairs);
  fprintf(snippet_file, "nomatch_pair_t const %s_nomatch_pairs[] =  {",
      index_name);
  for (i = 0; i < num_nomatch_pairs; i++) {
    fprintf(snippet_file, "{%d,%d,%s,%s},", nomatch_pairs[i].reg1,
        nomatch_pairs[i].reg2, tag2str(nomatch_pairs[i].tag1),
        tag2str(nomatch_pairs[i].tag2));
  }
  fprintf(snippet_file, "};\n");
}

static void
gen_peep_tab(char const *outfilename, char const *snippet_filename)
{
  FILE *outfile, *vars_fp, *constraints_fp;
  FILE *snippet_file, *nomatch_pairs_fp;
  cpu_constraints_t cpu_constraints;
  struct nomatch_pair_t nomatch_pairs[MAX_NOMATCH_PAIRS];
  int num_nomatch_pairs;
  int i;

  if (!load_object("in.o")) {
    exit(1);
  }

  vars_fp = xfopen("vars.ordered", "r");
  constraints_fp = xfopen("cpu_constraints", "r");
  nomatch_pairs_fp = xfopen("nomatch_pairs", "r");
  outfile = xfopen(outfilename, "w");
  snippet_file = xfopen(snippet_filename, "w");

  EXE_SYM *entries[nb_syms];
  int num_entries;
  num_entries = get_symtab_entries(entries, PEEP_PREFIX_STR);

  for (i = 0; i < num_entries - 1; i++) {
    assignments_t assignments;
    EXE_SYM *sym = entries[i], *next_sym = entries[i+1];
    char const *name = get_sym_name(sym);
    Elf32_Addr val  = sym->st_value;
    Elf32_Word size = next_sym->st_value - sym->st_value;

    read_assignments(vars_fp, name, &assignments);
    read_cpu_constraints(constraints_fp, name, &cpu_constraints);
    num_nomatch_pairs = read_nomatch_pairs(nomatch_pairs_fp, name,
        nomatch_pairs);

    if (strstart(name, SNIPPET_PREFIX_STR, NULL)) {
      gen_snippet_entry(snippet_file, name, &assignments, nomatch_pairs,
          num_nomatch_pairs);
    } else {
      gen_peeptab_entry(outfile, name, &text[val], size,
          &assignments, &cpu_constraints/*, &used_regs*/, nomatch_pairs,
          num_nomatch_pairs);
    }
  }
  fclose(vars_fp);
  fclose(outfile); fclose(snippet_file);
}

static void
parse_vname(char const *vname, char const *name, int *regtype, int *regno,
    int *regval)
{
  char const *ptr;
  char rbuf[8], *rptr;

  ptr = vname + strlen(name);
  ASSERT(*ptr == '_');
  ptr++;

  rptr = rbuf;
  while ((*rptr++ = *ptr++) != '_');
  *--rptr = '\0';
  if (!strcmp(rbuf, "r")) {
    *regtype = REGTYPE_REG;
  } else if (!strcmp(rbuf, "s")) {
    *regtype = REGTYPE_SEG;
  } else {
    printf("Unrecognized regtype %s.\n", rbuf);
    ABORT();
  }

  rptr = rbuf;
  while ((*rptr++ = *ptr++) != '_');
  *--rptr = '\0';
  *regno = atoi(rbuf);

  rptr = rbuf;
  while ((*rptr++ = *ptr++));
  *rptr = '\0';
  *regval = atoi(rbuf);
}

static bool
is_power_of_two(unsigned val, unsigned *bitpos)
{
  unsigned i;
  for (i = 0; (unsigned)(1<<i) < val; i++);
  if (val == (unsigned)(1 << i)) {
    *bitpos = i;
    return true;
  }
  return false;
}

/* Get the name of the REGNO'th register variable in ASSIGNMENTS. */
static void
get_regname(char *regname, size_t regname_size,assignments_t const *assignments,
    int regno)
{
  operand_t op;
  op.type = op_reg;
  op.val.reg = regno;
  op.tag.reg = tag_var;
  op.size = 4;
  op.rex_used = 0;
  operand2str(&op, regname, regname_size);
  ASSERT(regname[0] == '%');
  memmove(regname, regname + 1, strlen(regname));
}

/* Get the name of the SEGNO'th register variable in ASSIGNMENTS. */
static void
get_segname(char *segname, size_t segname_size,assignments_t const *assignments,
    int segno)
{
  operand_t op;
  op.type = op_seg;
  op.val.seg = segno;
  op.tag.seg = tag_var;
  op.size = 0;
  op.rex_used = 0;
  operand2str(&op, segname, segname_size);
  ASSERT(segname[0] == '%');
  memmove(segname, segname + 1, strlen(segname));
}



static bool
is_variable_name(char const *name, assignments_t const *assignments)
{
  int i;
  for (i = 0; i < assignments->num_assignments; i++) {
    char vname[32];
    operand2str(&assignments->arr[i].var, vname, sizeof vname);
    if (!strcmp(name, vname)) {
      return true;
    }
    if (strstart(name, "target_", NULL) && !strcmp(name+7, vname)) {
      return true;
    }
    if (vname[0] == '%' && !strcmp(name, vname+1)) {
      /* register variables. */
      return true;
    }
  }
  if (!strcmp(name, "tc_next_eip") || !strcmp(name, "gen_code_ptr")) {
    return true;
  }
  if (!strcmp(name, "gen_edge_ptr") || !strcmp(name, "fallthrough_addr")) {
    return true;
  }
  if (!strcmp(name, "edge2_end") || !strcmp(name, "edge1_end")) {
    return true;
  }
  return false;
}

/* Returns TRUE if it is necessary to emit an extern declaration. */
static bool
get_reloc_expr(char *name, size_t name_size, char const *sym_name,
    assignments_t const *assignments)
{
  if (is_variable_name(sym_name, assignments)) {
    ASSERT(name_size > strlen(sym_name));
    snprintf(name, name_size, "(long)%s", sym_name);
    return false;
  } else {
    snprintf(name, name_size, "(long)%s", sym_name);
    if (!strcmp(sym_name, "vcpu")) {
      return false;
    }
    if (!strcmp(sym_name, "monitor")) {
      return false;
    }
    return true;
  }
}

static void
output_joFF_values(FILE *outfile, EXE_SYM **entries, int entrynum,
    assignments_t const *assignments, unsigned code_len)
{
  EXE_SYM *sym = entries[entrynum], *next_sym = entries[entrynum+1];
  Elf32_Addr val = sym->st_value;
  Elf32_Word size = next_sym->st_value - sym->st_value;
  EXE_RELOC *rel;
  int i;

  for (i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
    unsigned long offset = get_rel_offset(rel);
    if (offset >= val && offset < val + size) {
      int reloc_offset;
      char const *sym_name;
      char relname[64];

      sym_name = get_rel_sym_name(rel);
      get_reloc_expr(relname, sizeof relname, sym_name, assignments);
      reloc_offset = rel->r_offset - val;
      if (strstr(relname, "target_C0")) {
        if (code_len > reloc_offset) {
          fprintf(outfile, "  joFF0 = (long)gen_code_ptr + %d;\n", reloc_offset);
        } else {
          fprintf(outfile, "  joFF0 = (long)gen_edge_ptr + %d;\n", reloc_offset - code_len);
        }
      }
      if (strstr(relname, "tc_next_eip")) {
        if (code_len > reloc_offset) {
          fprintf(outfile, "  joFF1 = (long)gen_code_ptr + %d;\n", reloc_offset);
        } else {
          fprintf(outfile, "  joFF1 = (long)gen_edge_ptr + %d;\n", reloc_offset - code_len);
          fprintf(outfile, "  edge1_end = joFF1 + 4;\n");
        }
      }
      if (strstr(relname, "end_Block")) {
        if (code_len > reloc_offset) {
          fprintf(outfile, "  joFF2 = (long)gen_code_ptr + %d;\n", reloc_offset);
        } else {
          fprintf(outfile, "  joFF2 = (long)gen_edge_ptr + %d;\n", reloc_offset - code_len);
        }
        fprintf(outfile, "  edge2_end = joFF2 + 4;\n");
        fprintf(outfile, "  *is_terminating = false;\n");
      }
    }
  }
}


static void
output_constant_variable_code(FILE *outfile, EXE_SYM **entries, int entrynum,
    assignments_t const *assignments, char const *optr, char const *eptr, unsigned code_size)
{
  EXE_SYM *sym = entries[entrynum], *next_sym = entries[entrynum+1];
  Elf32_Addr val = sym->st_value;
  Elf32_Word size = next_sym->st_value - sym->st_value;
  char const *name = get_sym_name(sym);
  EXE_RELOC *rel;
  int i;

  for (i = 0, rel = relocs; i < nb_relocs; i++, rel++) {
    unsigned long offset = get_rel_offset(rel);

    if (offset >= val && offset < val + size) {
      int reloc_offset, addend, type;
      char const *sym_name;
      char relname[64];

      sym_name = get_rel_sym_name(rel);
      get_reloc_expr(relname, sizeof relname, sym_name, assignments);
      reloc_offset = rel->r_offset - val;
      type = ELF32_R_TYPE(rel->r_info);

      switch (type) {
        case R_386_32:
          addend = *((uint32_t *)(text + rel->r_offset));
          if (reloc_offset < code_size) {
            fprintf(outfile, "  *(uint32_t *)(%s + %d) = "
              "%s + %d;\n", optr, reloc_offset, relname, addend);
          } else {
            fprintf(outfile, "  *(uint32_t *)(%s + %d) = "
              "%s + %d;\n", eptr, reloc_offset - code_size, relname, addend);
          }
          break;
        case R_386_16:
          addend = *((uint16_t *)(text + rel->r_offset));
          if (reloc_offset < code_size) {
            fprintf(outfile, "  *(uint16_t *)(%s + %d) = "
              "%s + %d;\n", optr, reloc_offset, relname, addend);
          } else {
            fprintf(outfile, "  *(uint16_t *)(%s + %d) = "
              "%s + %d;\n", eptr, reloc_offset - code_size, relname, addend);
          }
          break;
        case R_386_PC32:
          addend = *((uint32_t *)(text + rel->r_offset));
          if (reloc_offset < code_size) {
            fprintf(outfile, "  *(uint32_t *)(%s + %d) = "
              "%s - (long)(%s + %d) + %d;\n", optr, reloc_offset,
              relname, optr, reloc_offset, addend);
          } else {
            fprintf(outfile, "  *(uint32_t *)(%s + %d) = "
              "%s - (long)(%s + %d) + %d;\n", eptr, reloc_offset - code_size,
              relname, eptr, reloc_offset - code_size, addend);
          }
          break;
        default:
          ERR("Unsupported i386 relocation (%d) for %s %s\n", type, sym_name, relname);
          exit(1);
      }
    }
  }
}

static bool
is_identical_byte(uint8_t const *code[], size_t size, size_t bytenum)
{
  bool know_byteval = false;
  uint8_t byteval;
  size_t i;

  for (i = 0; i < NUM_SEGS; i++) {
    if (code[i]) {
      if (!know_byteval) {
        byteval = code[i][bytenum];
        know_byteval = true;
        continue;
      }
      ASSERT(know_byteval);
      if (code[i][bytenum] != byteval) {
        return false;
      }
    }
  }
  return true;
}

static bool
is_seg_prefix_byte(uint8_t const *code[], size_t size, size_t bytenum)
{
  uint8_t prefix_bytes[] = {0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65};
  size_t i;
  for (i = 0; i < NUM_SEGS; i++) {
    if (code[i]) {
      if (code[i][bytenum] != prefix_bytes[i]) {
        return false;
      }
    }
  }
  return true;
}

static size_t
output_regbit_code(char *buf, size_t buf_size, uint8_t const *code[],
    size_t size, int regtype, int bitnum, bool set, char const *optr, char const *eptr, unsigned code_size)
{
  uint8_t xor[size];
  int i, num_regbits;
  char *ptr = buf, *end = buf + buf_size;
	unsigned b;

  is_power_of_two(NUM_REGS, &num_regbits);
  ASSERT((1<<num_regbits) == NUM_REGS);
  i = bitnum;
  for (b = 0; b < size; b++) {
    uint8_t const *code_set = NULL, *code_reset = NULL;
    int j;

    if (   is_identical_byte(code, size, b)
        || (regtype == REGTYPE_SEG && is_seg_prefix_byte(code, size, b))) {
      continue;
    }

    if (code[0] && code[1 << i]) {
      code_set = code[1 << i];
      code_reset = code[0];
    } else {
      for (j = 0; j < num_regbits; j++) {
        if (j != i && code[1 << j] && code[(1 << j) + (1 << i)]) {
          code_set = code[(1 << j) + (1 << i)];
          code_reset = code[1 << j];
          break;
        }
      }
      if (j == num_regbits) {
        /*printf("Warning: Code reset/set pair not found for bitnum %d. \n"
				  "You should not be calling this function for this bitnum \n"
				  "if you have a corresponding regcons in the rule.\n", i);*/
				return 0;
      }
    }
    ASSERT(code_set);       ASSERT(code_reset);
    ASSERT(code_set[b] >= code_reset[b]);
    xor[b] = code_set[b]^code_reset[b];
    ASSERT((xor[b] | code_set[b]) == code_set[b]);
    ASSERT((xor[b] | code_reset[b]) == code_set[b]);

    if (xor[b]) {
      if (set) {
        if (b < code_size) {
          ptr += snprintf(ptr, end - ptr,
            "    *(%s + %d) |= 0x%hhx;\n", optr, b, xor[b]);
        } else {
          ptr += snprintf(ptr, end - ptr,
            "    *(%s + %d) |= 0x%hhx;\n", eptr, b - code_size, xor[b]);
        }
      } else {
        if (b < code_size) {
          ptr += snprintf(ptr, end - ptr,
            "    *(%s + %d) &= ~0x%hhx;\n", optr, b, xor[b]);
        } else {
          ptr += snprintf(ptr, end - ptr,
            "    *(%s + %d) &= ~0x%hhx;\n", eptr, b - code_size, xor[b]);
        }
      }
    }
  }
  ASSERT(ptr < end);
  return ptr - buf;
}

static void
output_regvar_code(FILE *outfile, EXE_SYM **entries, int entrynum,
    int num_variants, assignments_t const *assignments, int regtype,
    int regno, char const *optr, char const *eptr, unsigned code_size)
{
  EXE_SYM *sym = entries[entrynum], *next_sym = entries[entrynum+1];
  Elf32_Word size = next_sym->st_value - sym->st_value;
  uint8_t const *code[max(NUM_REGS, NUM_REGS)];
  char const *name = get_sym_name(sym);
  char regname[32];
  int i, num_regbits;
  char buf[4096];

  is_power_of_two(NUM_REGS, &num_regbits);
  ASSERT((1 << num_regbits) == NUM_REGS);
  memset(code, 0, sizeof code);

  if (regtype == REGTYPE_REG) {
    get_regname(regname, sizeof regname, assignments, regno);
  } else {
    ASSERT(regtype == REGTYPE_SEG);
    get_segname(regname, sizeof regname, assignments, regno);
  }

  /* First get the code for all regvals. */
  for (i = entrynum + 1; i < entrynum + num_variants; i++) {
    EXE_SYM *vsym = entries[i], *next_vsym = entries[i+1];
    Elf32_Addr vval = vsym->st_value;
    Elf32_Word vsize = next_vsym->st_value - vsym->st_value;
    char const *vname = get_sym_name(vsym);
    int vregtype, vregno, regval, bitpos;

    parse_vname(vname, name, &vregtype, &vregno, &regval);
    if (vregtype != regtype || vregno != regno) {
      continue;
    }
    code[regval] = &text[vval];
    if (vsize != size) {
      printf("vsize[%d] != size[%d] for %s:%d:%d:%d\n", vsize, size, vname,
          regtype, regno, regval);
      ABORT();
    }
  }

  /* Then generate code for each bitpos. */
  for (i = 0; i < num_regbits; i++) {
    if (output_regbit_code(buf, sizeof buf, code, size, regtype, i, true,
          optr, eptr, code_size)) {
      fprintf(outfile, "  if (%s & (1 << %d)) {\n", regname, i);
      fprintf(outfile, "%s", buf);
      fprintf(outfile, "  } else {\n");
      output_regbit_code(buf, sizeof buf, code, size, regtype, i, false, optr, eptr, code_size);
      fprintf(outfile, "%s", buf);
      fprintf(outfile, "  }\n");
    }
  }
  if (regtype == REGTYPE_SEG) {
    bool seg_prefix_present = false;
    for (i = 0; i < NUM_SEGS; i++) {
      if (code[i]) {
        unsigned b;
        for (b = 0; b < size; b++) {
          if (is_seg_prefix_byte(code, size, b)) {
            seg_prefix_present = true;
          }
        }
      }
    }
    if (seg_prefix_present) {
      fprintf(outfile, "  switch(%s) {\n", regname);
      for (i = 0; i < NUM_SEGS; i++) {
        unsigned b;
        fprintf(outfile, "    case %d: {\n", i);
        if (code[i]) {
          for (b = 0; b < size; b++) {
            if (is_seg_prefix_byte(code, size, b)) {
              if (b < code_size) {
                fprintf(outfile, "      *(%s + %d) = 0x%hhx;\n", optr, b,
                  code[i][b]);
              } else {
                fprintf(outfile, "      *(%s + %d) = 0x%hhx;\n", eptr, b - code_size,
                  code[i][b]);
              }
            }
          }
        } else {
          fprintf(outfile, "      ABORT();\n");
        }
        fprintf(outfile, "    }\n");
        fprintf(outfile, "    break;\n");
      }
      fprintf(outfile, "  }\n");
    }
  }
}


typedef struct {
  int regtype, regno;
  struct hash_elem h_elem;
} entry_t;

static bool
entry_equal(struct hash_elem const *ha, struct hash_elem const *hb, void *aux)
{
  entry_t *a = hash_entry(ha, entry_t, h_elem);
  entry_t *b = hash_entry(hb, entry_t, h_elem);
  return (a->regtype == b->regtype && a->regno == b->regno);
}

static unsigned
entry_hash(struct hash_elem const *ha, void *aux)
{
  entry_t *a = hash_entry(ha, entry_t, h_elem);
  return a->regtype * 100 + a->regno;
}

static void
entry_free(struct hash_elem *ha, void *unused)
{
  entry_t *a = hash_entry(ha, entry_t, h_elem);
  free(a);
}

static void
output_register_variable_code(FILE *outfile, EXE_SYM **entries, int entrynum,
    int num_variants, assignments_t const *assignments, char const *optr, char const *eptr, 
    unsigned code_size)
{
  EXE_SYM *sym = entries[entrynum], *next_sym = entries[entrynum + 1];
  Elf32_Word size = next_sym->st_value - sym->st_value;
  char const *name = get_sym_name(sym);
  entry_t entry, *new_entry;
  struct hash hash;
  int i;
  
  hash_init(&hash, entry_hash, entry_equal, NULL);
  for (i = entrynum + 1; i < entrynum + num_variants; i++) {
    EXE_SYM *vsym = entries[i], *next_vsym = entries[i + 1];
    Elf32_Word vsize = next_vsym->st_value - vsym->st_value;
    char const *vname = get_sym_name(vsym);
    int regtype, regno, regval;

    if (vsize != size) {
      ERR("Output codesize for entry '%s' (%d) differs from it's variant "
          "'%s' (%d)\n", name, size, vname, vsize);
      exit(1);
    }
    parse_vname(vname, name, &regtype, &regno, &regval);

    entry.regtype = regtype;
    entry.regno = regno;
    if (hash_find(&hash, &entry.h_elem)) {
      continue;
    }
    new_entry = malloc(sizeof(entry_t));
    ASSERT(new_entry);
    new_entry->regtype = regtype;
    new_entry->regno = regno;
    hash_insert(&hash, &new_entry->h_elem);

    output_regvar_code(outfile, entries, entrynum, num_variants, assignments,
        regtype, regno, optr, eptr, code_size);
  }
  hash_destroy(&hash, entry_free);
}

static void
output_edge_offsets(FILE *outfile, EXE_SYM *entry, size_t entry_size,
    uint16_t *edge_offsets)
{
  int edgenum;
  edge_offsets[0] = edge_offsets[1] = edge_offsets[2] = 0xffff;
  for (edgenum = 0; edgenum <= 2; edgenum++) {
    EXE_SYM *edges[nb_syms];
    int i, nb_edges;
    char edge_str[64];

    snprintf(edge_str, sizeof edge_str, ".edge%d", edgenum);
    nb_edges = get_symtab_entries(edges, edge_str);
    for (i = 0; i < nb_edges; i++) {
      char const *edge_name = get_sym_name(edges[i]);
      char const *entry_name = get_sym_name(entry);
      Elf32_Addr edge_val = edges[i]->st_value;
      Elf32_Word entry_val = entry->st_value;

      if (edge_val >= entry_val && edge_val < entry_val + entry_size) {
        ASSERT(strstr(edge_name, entry_name));
        /*fprintf(outfile, "  edge_offset[%d] = %d;\n", edgenum,
            edge_val - entry_val);*/
        edge_offsets[edgenum] = edge_val - entry_val;
      }
    }
  }
}

static int
get_num_variants(EXE_SYM **entries, char const *name)
{
  char const *sym_variant;
  int num_variants;
  for (num_variants = 1;
      strstart(get_sym_name(entries[num_variants]), name, &sym_variant)
      && *sym_variant == '_';
      num_variants++) {
  }
  return num_variants;
}



static int
output_gencode_for_entry(FILE *outfile, FILE *codefile, EXE_SYM **entries,
    int i, char const *prefix, assignments_t const *assignments,
    char const *optr, char const *eptr, unsigned code_size)
{
  EXE_SYM *sym = entries[i], *next_sym = entries[i+1];
  Elf32_Addr val = sym->st_value;
  Elf32_Word size = next_sym->st_value - sym->st_value;
  char const *name = get_sym_name(sym);
  char regs[NUM_REGS];
  int num_regs = 0;
	unsigned j;
  int num_variants;
  int edge_size = 0;

  if (size == 0) {
    return 0;
  }
  ASSERT(size > 0);

  if (code_size == 0xffff) {
    code_size = size;
  } else {
    assert(code_size < size);
    edge_size = size - code_size;
  }
  num_variants = get_num_variants(&entries[i], name);

  fprintf(codefile, ".global %s_code\n%s_code:\n", name, name);
  fprintf(codefile, ".byte ");
  for (j = 0; j < size; j++) {
    fprintf(codefile, "%#x%c", text[val + j], (j == size-1)?'\n':',');
  }

  fprintf(outfile, "  memcpy(%s, %s_code, %d);\n", optr, name, code_size);
  if (edge_size) {
    fprintf(outfile, "  memcpy(%s, %s_code + %d, %d);\n", eptr, name, code_size, edge_size);
  }

  output_joFF_values(outfile, entries, i, assignments, code_size);
  output_register_variable_code(outfile, entries, i, num_variants,
      assignments, optr, eptr, code_size);
  output_constant_variable_code(outfile, entries, i, assignments, optr, eptr, code_size);
  fprintf(outfile, "  %s += %d;\n", optr, code_size);
  if (edge_size) {
    fprintf(outfile, "  %s += %d;\n", eptr, size - code_size);
  }
  return num_variants;
}


static void
output_gencode_and_rollback_from_file(FILE *outfile, FILE *codefile,
    char const *filename, char const *prefix)
{
  int i;
  FILE *vars_fp;
  int num_variants;
  assignments_t assignments;
  int num_entries;
  uint16_t edges[3];

  if (!(vars_fp = fopen("vars.ordered", "r"))) {
    ERR("fopen '%s' failed. %s\n", "vars.ordered", strerror(errno));
    exit(1);
  }

  if (!load_object(filename)) {
    exit(1);
  }
  EXE_SYM *entries[nb_syms];
  num_entries = get_symtab_entries(entries, prefix);

  for (i = 0; i < num_entries - 1; i += num_variants) {
    EXE_SYM *sym = entries[i];
    char const *name = get_sym_name(sym);
    EXE_SYM *next_sym = entries[i+1];
    Elf32_Addr val = sym->st_value;
    Elf32_Word size = next_sym->st_value - sym->st_value;
    Elf32_Addr excp_val;
    int j, s;
    unsigned code_size = 0xffff;
    read_assignments(vars_fp, name, &assignments);

    fprintf(outfile, "case %s: {\n", name);
    fprintf(outfile, "  void %s_code(void);\n", name);
    for (j = 0; j < assignments.num_assignments; j++) {
      char opstr[32];
      operand2str(&assignments.arr[j].var, opstr, sizeof opstr);
      if (opstr[0] == '%') {
        memmove(opstr, opstr + 1, strlen(opstr));
      }
      fprintf(outfile, "  %s = *peep_param_ptr++;\n", opstr);
    }

    output_edge_offsets(outfile, sym, size, edges);

    for (j = 0; j < 3; j++) {
      if (edges[j] != 0xffff && code_size > edges[j]) {
        code_size = edges[j];
      }
    }
    if (edges[0] != 0xffff) {
      fprintf(outfile, "  target_C0 = (long)gen_edge_ptr + %d;\n",
          (int)edges[0] - code_size);
    }
    if (edges[1] != 0xffff) {
      fprintf(outfile, "  tc_next_eip = (long)gen_edge_ptr + %d;\n",
          (int)edges[1] - code_size);
    }
    if (edges[2] != 0xffff) {
      fprintf(outfile, "  end_Block = (long)gen_edge_ptr + %d;\n",
          (int)edges[2] - code_size);
    }
    num_variants = output_gencode_for_entry(outfile, codefile, entries, i,
        prefix, &assignments, "gen_code_ptr", "gen_edge_ptr", code_size);
    fprintf(outfile, "}\nbreak;\n\n");
  }
  fclose(vars_fp);
}

static void
output_gencode(char const *outfilename)
{
  EXE_SYM *sym;
  FILE *outfile, *codefile;

  outfile = xfopen(outfilename, "w");
  codefile = xfopen("peepgen_outcode.S", "w");

  fprintf(outfile, "void\npeepgen_code(btmod_vcpu *v, int label, "
      "long *peep_param_buf, uint8_t **gen_code_buf, uint8_t **gen_edge_buf, "
			"unsigned long *tx_target, long fallthrough_addr, bool *is_terminating)\n");
  fprintf(outfile, "{\n");
  fprintf(outfile, "vcpu_t *vcpu = &v->vcpu;\n");
  fprintf(outfile, "long read__mask_b = (long)&read_mask_b;\n");
  fprintf(outfile, "long read__mask_w = (long)&read_mask_w;\n");
  fprintf(outfile, "long read__mask_l = (long)&read_mask_l;\n");
  fprintf(outfile, "long write__mask_b = (long)&write_mask_b;\n");
  fprintf(outfile, "long write__mask_w = (long)&write_mask_w;\n");
  fprintf(outfile, "long write__mask_l = (long)&write_mask_l;\n");
  fprintf(outfile, "long *peep_param_ptr = peep_param_buf;\n");
  fprintf(outfile, "uint8_t *gen_code_ptr = *gen_code_buf;\n");
  fprintf(outfile, "uint8_t *gen_edge_ptr = *gen_edge_buf;\n");
  fprintf(outfile, "long vr0d=-1, vr1d=-1, vr2d=-1, vseg0=-1, joFF0, joFF1, joFF2, "
			"C0, C1, C2, target_C0, tc_next_eip, end_Block, edge1_end, edge2_end, tr0d=-1, tr1d=-1, tr2d=-1;\n\n");
  fprintf(outfile, "(void)vr2d;\n\n");
  fprintf(outfile, "(void)tr2d;\n\n");
  fprintf(outfile, "(void)C1;\n\n");
  fprintf(outfile, "(void)C2;\n\n");
  fprintf(outfile, "(void)edge1_end;\n\n");

  fprintf(outfile, "\nswitch (label) {\n");
	fprintf(outfile, "case peep_null: break;\n");

  output_gencode_and_rollback_from_file(outfile, codefile, "out.o",
      PEEP_PREFIX_STR);

  fprintf(outfile, "}\n");    /* switch. */
  fprintf(outfile, "*gen_code_buf = gen_code_ptr;\n");
  fprintf(outfile, "*gen_edge_buf = gen_edge_ptr;\n");
  fprintf(outfile, "}\n");    /* function close. */
  fclose(codefile);
  fclose(outfile);
}

static bool
insns_are_isomorphic(struct insn_t const *a, struct insn_t const *b)
{
  int i;
  if (a->opc != b->opc) {
    printf("opc's not equal.\n");
    return false;
  }
  for (i = 0; i < 3; i++) {
    if (a->op[i].type != b->op[i].type) {
      printf("types not equal.\n");
      return false;
    }
    if (a->op[i].size != b->op[i].size) {
      printf("sizes not equal.\n");
      return false;
    }
    if (a->op[i].type == op_mem) {
      if (a->op[i].val.mem.addrsize != a->op[i].val.mem.addrsize) {
        return false;
      }
    }
  }
  return true;
}

static bool
insn_seqs_are_isomorphic(insn_t const *a, int na, insn_t const *b, int nb)
{
  int i;
  if (na != nb) {
    return false;
  }
  for (i = 0; i < na; i++) {
    if (!insns_are_isomorphic(&a[i], &b[i])) {
      printf("insn %d is not isomorphic.\n", i);
      return false;
    }
  }
  return true;
}

#define MAX_OUT_N_INSNS 512

#define pattern_match_field(op, c_op, field, regnum, default_regval,cur_regval)\
  do {                                                                         \
    if (op->tag.field != tag_const && op->val.field == regnum) {               \
      ASSERT(c_op->tag.field == tag_const);                                    \
      if (c_op->val.field == cur_regval) {                                     \
        /* things are OK (as expected). do nothing. */                         \
      } else {                                                                 \
        if (c_op->tag.field != default_regval) {                               \
          printf("Register variant has an unexpected register value.\n");      \
          ABORT();                                                             \
        } else {                                                               \
          /* This must be a constant which we mis-interpreted as a variable.   \
           * Rename it back to it's constant value. */                         \
          op->tag.field = tag_const;                                           \
          op->val.field = default_regval;                                      \
        }                                                                      \
      }                                                                        \
    } else if (op->tag.field == tag_const) {                                   \
      ASSERT(op->val.field == c_op->val.field);                                \
    }                                                                          \
  } while(0)

static void
insn_pattern_match_regvar(insn_t *insn, insn_t const *c_insn, int regnum,
    int default_regval, int cur_regval)
{
  int i;
  for (i = 0; i < 3; i++) {
    operand_t *op = &insn->op[i];
    operand_t *c_op = &insn->op[i];

    ASSERT(op->type == c_op->type);
    if (op->type == op_mem) {
      pattern_match_field(op, c_op, mem.base, regnum, default_regval,
          cur_regval);
      pattern_match_field(op, c_op, mem.index, regnum, default_regval,
          cur_regval);
    } else if (op->type == op_reg) {
      pattern_match_field(op, c_op, reg, regnum, default_regval, cur_regval);
    }
  }
}

static void
insn_seq_pattern_match_regvar(insn_t *insns, insn_t const *c_insns,
    int n_insns, int regnum, int default_regval, int cur_regval)
{
  int i;
  for (i = 0; i < n_insns; i++) {
    insn_pattern_match_regvar(&insns[i], &c_insns[i], regnum, default_regval,
        cur_regval);
  }
}

#define regvar_union_on_memfield(op, v_op, field) do {                      \
  if (v_op->tag.field != tag_const && op->tag.field == tag_const) {         \
    op->val.field = v_op->val.field;                                        \
    op->tag.field = v_op->tag.field;                                        \
  }                                                                         \
} while (0)

static void
insn_regvar_union(insn_t *insn, insn_t const *v_insn)
{
  int i;
  for (i = 0; i < 3; i++) {
    operand_t *op = &insn->op[i];
    operand_t const *v_op = &v_insn->op[i];
    ASSERT(op->type == v_op->type);
    if (v_op->type == op_reg) {
      regvar_union_on_memfield(op, v_op, reg);
    } else if (v_op->type == op_mem) {
      regvar_union_on_memfield(op, v_op, mem.base);
      regvar_union_on_memfield(op, v_op, mem.index);
    }
  }
}

static void
insns_regvar_union(insn_t *insns, insn_t const *r_insns, int n_insns)
{
  int i;
  for (i = 0; i < n_insns; i++) {
    insn_regvar_union(&insns[i], &r_insns[i]);
  }
}

static int
identify_reg_variables_in_outcode(char const *name, insn_t *insns,
    size_t n_insns, EXE_SYM **entries)
{
  EXE_SYM *sym = entries[0], *next_sym = entries[1];
  Elf32_Addr val = sym->st_value;
  Elf32_Word size = next_sym->st_value - sym->st_value;
  char const *vname = get_sym_name(sym);
  uint8_t *code = &text[val];
  insn_t r_insns[MAX_OUT_N_INSNS];
  unsigned num_r_insns;
  assignments_t assignments;
  operand_t regvar, regcons;
  int i, num_variants;
  int regtype, regnum, regval;
  char rname[strlen(name) + 64];
  char const *rval_str;

  parse_vname(vname, name, &regtype, &regnum, &regval);
  snprintf(rname, sizeof rname, "%s_r_%d_", name, regnum);
  num_variants = get_num_variants(entries, name);
  if (regtype == REGTYPE_SEG) {
    return num_variants;
  }
  ASSERT(regtype == REGTYPE_REG);
  ASSERT(strstart(vname, rname, NULL));
  num_r_insns = disas_insns(r_insns, sizeof r_insns/sizeof r_insns[0], code,
      size, 4);
  if (num_r_insns == 0) {
    return 0;
  }
  ASSERT(insn_seqs_are_isomorphic(insns, n_insns, r_insns, num_r_insns));
  ASSERT(regval >= 0 && regval < NUM_REGS);
  regvar.type = regcons.type = op_reg;
  regvar.size = regcons.size = 4;
  regvar.rex_used = regcons.rex_used = 0;
  regvar.val.reg = regnum;
  regcons.val.reg = regval;
  regvar.tag.reg = tag_var;
  regcons.tag.reg = tag_const;
  assignments_init(&assignments);
  assignments_add(&assignments, &regvar, &regcons);
  insns_rename_constants(r_insns, num_r_insns, &assignments);
  ASSERT(n_insns == num_r_insns);
  insns_regvar_union(insns, r_insns, n_insns);

  for (num_variants = 1;
      strstart(get_sym_name(entries[num_variants]), rname, &rval_str);
      num_variants++) {
    EXE_SYM *sym = entries[num_variants], *next_sym = entries[num_variants+1];
    Elf32_Addr val = sym->st_value;
    Elf32_Word size = next_sym->st_value - sym->st_value;
    char const *vname = get_sym_name(sym);
    insn_t rv_insns[MAX_OUT_N_INSNS];
    uint8_t *code = &text[val];
    int rval = atoi(rval_str);
    unsigned num_rv_insns;

    ASSERT(rval != regval);
    num_rv_insns = disas_insns(rv_insns, sizeof rv_insns/sizeof rv_insns[0],
        code, size, 4);
    //ASSERT(insn_seqs_are_isomorphic(insns, n_insns, rv_insns, num_rv_insns));
    ASSERT(num_rv_insns == n_insns);
    insn_seq_pattern_match_regvar(rv_insns, insns, n_insns,
        regnum, regval, rval);
  }
  return num_variants;
}

static int
output_nomatch_pairs_for_entry(FILE *fp, EXE_SYM **entries, int i,
    char const *prefix, assignments_t const *assignments)
{
  EXE_SYM *sym = entries[i], *next_sym = entries[i+1];
  Elf32_Addr val = sym->st_value;
  Elf32_Word size = next_sym->st_value - sym->st_value;
  char const *name = get_sym_name(sym);
  struct nomatch_pair_t nomatch_pairs[MAX_NOMATCH_PAIRS];
  insn_t insns[MAX_OUT_N_INSNS];
  regset_t use;
  int n_insns;
  int num_nomatch_pairs = 0;
  char const *sym_variant;
  int num_variants;
  uint8_t *code;

  if (size == 0) {
    return 0;
  }
  ASSERT(size > 0);

  fprintf(fp, "%s:\n", name);
  code = &text[val];
  num_variants = get_num_variants(&entries[i], name);
  n_insns = disas_insns(insns, sizeof insns/sizeof insns[0], code, size, 4);

  for (num_variants = 1;
      strstart(get_sym_name(entries[i + num_variants]), name, &sym_variant)
			&& *sym_variant == '_';) {
    num_variants += identify_reg_variables_in_outcode(name, insns, n_insns,
        &entries[i + num_variants]);
  }

  regset_clear_all(&use);
  for (i = n_insns - 1; i >= 0; i--) {
    regset_t iuse, idef;

    insn_get_usedef(&insns[i], &iuse, &idef);

    char insnbuf[512], use_regsetbuf[512], def_regsetbuf[512];
    insn2str(&bt, &insns[i], insnbuf, sizeof insnbuf);
    regset2str(&iuse, use_regsetbuf, sizeof use_regsetbuf);
    regset2str(&idef, def_regsetbuf, sizeof def_regsetbuf);
    num_nomatch_pairs =
      regset_output_nomatch_pairs(nomatch_pairs, MAX_NOMATCH_PAIRS,
          num_nomatch_pairs, &idef, &use);
    regset_diff(&use, &idef);
    regset_union(&use, &iuse);
  }
  char buf[4096];
  char *ptr = buf, *end = ptr + sizeof buf;
  strncpy(ptr, "none", end - ptr);
  for (i = 0; i < num_nomatch_pairs; i++) {
    ptr += nomatch_pair2str(&nomatch_pairs[i], ptr, end - ptr);
  }
  fprintf(fp, "%s\n", buf);
  return num_variants;
}

static void
output_nomatch_pairs(char const *filename)
{
  FILE *nomatch_pairs_fp, *vars_fp;
  int num_variants, num_entries, i;
  assignments_t assignments;
  char const *prefix = PEEP_PREFIX_STR;

  nomatch_pairs_fp = xfopen("nomatch_pairs", "w");

  if (!(vars_fp = fopen("vars.ordered", "r"))) {
    ERR("fopen '%s' failed. %s\n", "vars.ordered", strerror(errno));
    exit(1);
  }
  if (!load_object("out.o")) {
    exit(1);
  }
  EXE_SYM *entries[nb_syms];
  num_entries = get_symtab_entries(entries, prefix);

  for (i = 0; i < num_entries - 1; i += num_variants) {
    EXE_SYM *sym = entries[i];
    char const *name = get_sym_name(sym);
    EXE_SYM *next_sym = entries[i + 1];
    Elf32_Addr val = sym->st_value;
    Elf32_Word size = next_sym->st_value - sym->st_value;

    read_assignments(vars_fp, name, &assignments);
    num_variants = output_nomatch_pairs_for_entry(nomatch_pairs_fp, entries,
        i, prefix, &assignments);
  }
  fclose(nomatch_pairs_fp);
}

static void
parse_snippet_constraints(char *constraints, size_t constraints_len,
    char const *comment)
{
  char *buf = constraints;
  char const *comma;
  ASSERT(*comment == ';');
  snprintf(constraints, constraints_len, "");
  comment++;
  comment = strstr(comment, "//");
  if (!comment) {
    return;
  }
  comment+=2;
  while (comma = strchr(comment, ',')) {
    strncpy(constraints, comment, comma - comment);
    constraints += comma - comment;
    comment = comma + 1;
    *constraints++ = '\n';
  }
  strncpy(constraints, comment, strlen(comment));
  constraints += strlen(comment);
  *constraints++ = '\n';
  *constraints++ = '\0';
  ASSERT(strlen(constraints) < constraints_len);
}

static void
append_code_snippets(char const *filename, char const *append_filename)
{
  FILE *fp, *outfile;
  char line[512], code_snippet[512], capitals[512], comment[512];
  char constraints[512];
  char const *prefix;
  int linenum = 0;
	unsigned i;

  fp = xfopen(filename, "r");
  outfile = xfopen(append_filename, "a");

  while (fscanf(fp, "%[^\n]\n", line) != EOF) {
    ASSERT(strlen(line) + 1 < sizeof line);
    linenum++;
    if (sscanf(line, "extern peepgen_label_t %[^;]%[^\n]\n", code_snippet,
          comment) == 2) {
      fprintf(outfile, "%s:\n", code_snippet);
      fprintf(outfile, "  nop\n  --\n");
      parse_snippet_constraints(constraints, sizeof constraints, comment);
      fprintf(outfile, "%s", constraints);
      fprintf(outfile, "  --\n");
      prefix = strstr(code_snippet, SNIPPET_PREFIX_STR);
      if (!prefix || prefix != code_snippet) {
        goto scan_error;
      }
      strncpy(capitals, code_snippet + strlen(SNIPPET_PREFIX_STR),
          sizeof capitals);
      for (i = 0; i < strlen(capitals); i++) {
        capitals[i] = toupper(capitals[i]);
      }
      fprintf(outfile, "    %s\n", capitals);
      fprintf(outfile, "  ==\n");
    } else if (line[0] == '#' || strstr(line, "typedef") == line) {
    } else {
scan_error:
      printf("Scan error at %s:%d\n%s\n", filename, linenum, line);
      unlink(append_filename);
      exit(1);
    }
  }
  fclose(fp);
  fclose(outfile);
}

int
main(int argc, char **argv)
{
  char const *filename, *outfilename;
  int c, out_type;
  FILE *fp;
	bt.v = (btmod_vcpu *)kmalloc(sizeof(btmod_vcpu), GFP_KERNEL);
	v = bt.v;
	v->bt = &bt;
  enum {
    OUT_ASFILES = 0,
    OUT_DEFS,
    OUT_PEEPTAB,
    OUT_GENCODE,
    OUT_GENOFFSETS,
    OUT_CODE_SNIPPETS,
    OUT_NOMATCH_PAIRS,
    OUT_VARS_ORDERED,
  };

  outfilename = "out.c";
  out_type = OUT_ASFILES;
  for (;;) {
    c = getopt(argc, argv, "ho:r:a:tgfdsnv");
    if (c == -1) {
      break;
    }
    switch(c) {
      case 'h':
        usage();
        break;
      case 'o':
        outfilename = optarg;
        break;
      case 'a':
        outfilename = optarg;
        break;
      case 'd':
        out_type = OUT_DEFS;
        break;
      case 't':
        out_type = OUT_PEEPTAB;
        break;
      case 'g':
        out_type = OUT_GENCODE;
        break;
      case 'f':
        out_type = OUT_GENOFFSETS;
        break;
      case 's':
        out_type = OUT_CODE_SNIPPETS;
        break;
      case 'v':
        out_type = OUT_VARS_ORDERED;
        break;
      case 'n':
        out_type = OUT_NOMATCH_PAIRS;
        break;
    }
  }
  if (c == 't' && optind >= argc)
    usage();
  filename = argv[optind];
  switch (out_type) {
    case OUT_GENOFFSETS:
      gen_offsets();
      break;
    case OUT_ASFILES:
			ASSERT(filename);
      gen_as_files(filename);
      break;
    case OUT_DEFS:
      gen_defs(outfilename);
      break;
    case OUT_PEEPTAB:
      opc_init(v);
      opctable_print(&bt);
      gen_peep_tab(outfilename, "gen_snippets.h");
      break;
    case OUT_VARS_ORDERED:
      opc_init(v);
      gen_vars_ordered(outfilename);
      break;
    case OUT_GENCODE:
      output_gencode(outfilename);
      break;
    case OUT_NOMATCH_PAIRS:
      opc_init(v);
      output_nomatch_pairs(outfilename);
      break;
    case OUT_CODE_SNIPPETS:
      append_code_snippets(filename, outfilename);
      break;
    default:
      assert(0);
  }
	free(bt.v);
  return 0;
}
