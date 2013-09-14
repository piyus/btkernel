#include "peep/peep.h"
#include <linux/types.h>
#include <utils.h>
#include <linux/string.h>
#include <debug.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/mm.h>
#include <types.h>
#include "peep/i386-dis.h"
#include "peep/insntypes.h"
#include "peep/insn.h"
#include "peep/peeptab.h"
#include "peep/assignments.h"
#include "peep/regset.h"
#include "peep/tb.h"
#include "peep/cpu_constraints.h"
#include "peep/sti_fallthrough.h"
#include "sys/vcpu.h"
#include "btmod.h"
#include "btfix.h"
#include "hypercall.h"
#include "peep/instrument.h"
#include "peep/gvtable.h"

typedef enum {
  peep_null = 0,
#define CONCAT(a,b) a ## b
#define _DEF(p, x) CONCAT(p, x),
#define DEF(x) _DEF(PEEP_PREFIX, x)
#include "peepgen_defs.h"
#undef DEF
#undef _DEF
#undef CONCAT
} peepgen_label_t;

#include "peepgen_gencode.h"

static void
vpeepgen_emit_code(btmod_vcpu *v, uint8_t **obuf, uint8_t **ebuf,
    peepgen_label_t label, int n_args, va_list args, bool *is_term)
{
  long params[n_args];
  int i;

  for (i = 0; i < n_args; i++) {
    params[i] = va_arg(args, long);
  }
  peepgen_code(v, label, params, obuf, ebuf, &tx_target, 0, is_term);
}

static void
peepgen_emit_code(btmod_vcpu *v, uint8_t **obuf, uint8_t **ebuf,
    bool *is_term, int label, int n_args, ...)
{
  va_list args;
  va_start(args, n_args);
  vpeepgen_emit_code(v, obuf, ebuf, label, n_args, args, is_term);
  va_end(args);
}

static int
check_if_flags_are_dead(btmod_vcpu *v, uint32_t cur_addr)
{
  insn_t insn;
  int disas;

  do {
    disas = disas_insn(v, (uint8_t*)cur_addr, cur_addr, &insn, 4, true);
    ASSERT(disas);
    if (insn_reads_flags(v->bt, &insn)) {
			return 0;
    } else if (insn_sets_flags(v->bt, &insn)) {
      return 1;
    }
    if (insn_is_terminating(v->bt, &insn)) {
      return 0;
    }
    cur_addr += disas;
  } while (1);

  return 0;
}

static void
check_dead_flags_and_tregs(btmod_vcpu *v, uint32_t cur_addr, 
		int *treg, int *opt_treg, int *dead, bool rep_prefix, 
		insn_t const *insns, int n_regs)
{
  int r;
  insn_t insn;
  int disas;
	struct regset_t use;
  struct regset_t def;
	int treg_possible = 1;
	int dead_flag_possible = 1;
  int temp[NUM_REGS] = {1,1,1,1,0,1,1,1};

	*opt_treg = -1;
	*treg = -1;
	*dead = 0;

	if (rep_prefix) {
		temp[R_ECX] = 0;
		temp[R_ESI] = 0;
		temp[R_EDI] = 0;
		*treg = R_EBX;
	} else {
		__insn_get_usedef(v->bt, insns, &use, &def);
		for (r = 0; r < n_regs; r++) {
			if (temp[r] && use.cregs[r] == 0 && def.cregs[r] == 0) {
				*treg = r;
				break;
			}
		}
	}
  do {
    disas = disas_insn(v, (uint8_t*)cur_addr, cur_addr, &insn, 4, true);
    ASSERT(disas);

		if (dead_flag_possible) {
      if (insn_reads_flags(v->bt, &insn)) {
			  dead_flag_possible = 0;
      } else if (insn_sets_flags(v->bt, &insn)) {
			  dead_flag_possible = 0;
				*dead = 1;
      }
		}

    if (insn_is_terminating(v->bt, &insn) 
				|| (!dead_flag_possible && !treg_possible)) {
      break;
    }

		if (treg_possible) {
			treg_possible = 0;
		  __insn_get_usedef(v->bt, &insn, &use, &def);
		  for (r = 0; r < n_regs; r++) {
				if (temp[r] && use.cregs[r]) {
				  temp[r] = 0;
				}
			  if (temp[r]) {
					if (def.cregs[r]) {
						*opt_treg = r;
						*treg = r;
						treg_possible = 0;
						break;
					} else {
						treg_possible++;
					}
				}
		  }
		}

    cur_addr += disas;
  } while (1);
}

static int
calc_offset(int scale, int opcsize, int perm, int safe)
{
  int offset = 0;
	switch (scale) {
		case 0:
		case 1: break;
		case 2: offset += 6; break;
	  case 4: offset += 12; break;
		case 8: offset += 18; break;
		default: dprintk("scale=%d\n", scale); ASSERT(0);
	}
	switch (opcsize) {
		case 1: offset += (perm)?3:0; break;
		case 2: offset += (perm)?4:1; break;
		case 4: offset += (perm)?5:2; break;
    default: ASSERT(0);
	}
  if (safe) {
    offset += peep_snippet_remap_safe_1_b_r 
       - peep_snippet_remap_1_b_r;
  }
	return offset;
}

static void
gen_shadow_code(btmod_vcpu *v, uint8_t **optr, 
	uint8_t **eptr, insn_t const *insn, 
	operand_t const *op, int safe, int treg,
	int opc_size, uint32_t eip, bool *is_term)
{
	long shadow_disp;
	int perm;
	peepgen_label_t label;
  int base, index;

	base = op->val.mem.base;
	index = op->val.mem.index;
	perm = writes_operand(v->bt, insn, op);
	shadow_disp = (long)op->val.mem.disp + _shadowmem_start;
  label = calc_offset(op->val.mem.scale, opc_size, perm, safe);

  if (base != -1 && index != -1) {
		label += peep_snippet_remap_1_b_r;
    peepgen_emit_code(v, optr, eptr, is_term, 
				label, 5, base, index, treg, shadow_disp, eip);
  } else if (base != -1) {
		label += peep_snippet_rbase_b_r;
    peepgen_emit_code(v, optr, eptr, is_term, 
				label, 4, base, treg, shadow_disp, eip);
  } else if (index != -1) {
		label += peep_snippet_rindx_1_b_r;
    peepgen_emit_code(v, optr, eptr, is_term, 
				label, 4, index, treg, shadow_disp, eip);
  } else {
		label += peep_snippet_rdisp_b_r;
    peepgen_emit_code(v, optr, eptr, is_term, 
				label, 3, treg, shadow_disp, eip);
  }
}

static void
gen_shadow_str_code(btmod_vcpu *v, uint8_t **optr, uint8_t **eptr, 
		insn_t const *insn, int safe, int treg,
		int opc_size, uint32_t eip, bool *is_term)
{
	int off;
  char const *opc = opctable_name(v->bt, insn->opc);

	switch (opc_size) {
		case 1: off = 0; break;
		case 2: off = 1; break;
		case 4: off = 2; break;
		default: ASSERT(0);
	}
	if (safe) {
		off += peep_snippet_movss_b - peep_snippet_movs_b;
	}

  if (strstart(opc, "movs", NULL)) {
		peepgen_emit_code(v, optr, eptr, is_term, 
				peep_snippet_movs_b + off, 3, treg, _shadowmem_start, eip);
	} else if (strstart(opc, "stos", NULL)) {
		peepgen_emit_code(v, optr, eptr, is_term, 
				peep_snippet_stos_b + off, 3, treg, _shadowmem_start, eip);
	} else if (strstart(opc, "cmps", NULL)) {
		peepgen_emit_code(v, optr, eptr, is_term, 
				peep_snippet_cmps_b + off, 3, treg, _shadowmem_start, eip);
	} else if (strstart(opc, "scas", NULL)) {
		peepgen_emit_code(v, optr, eptr, is_term, 
				peep_snippet_scas_b + off, 3, treg, _shadowmem_start, eip);
	} else {
		dprintk("opc=%s\n", opc);
		ASSERT(0);
	}
}

bool
instrument_memory(btmod_vcpu *v, uint32_t eip, 
		insn_t const *insn, uint8_t **optr, 
		uint8_t **eptr, bool tmode)
{
  operand_t const *op1 = NULL, *op2 = NULL;
	int safe = 0;
	int dead_flags = 0;
	int treg = -1;
	bool rep_prefix = false;
	int opt_treg = -1;
	int opc_size;
	bool is_term;

  if (insn_accesses_mem(v->bt, insn, &op1, &op2)) {

		if (  op1->val.mem.base == R_ESP 
				|| op1->val.mem.base == R_EBP) {
			return 0;
		}

		if (insn_is_prefetch(v->bt, insn)) {
      return 0;
		}

		if (op1->val.mem.seg.sel == R_FS) {
      return 0;
		}

		if (op1->val.mem.seg.sel == R_GS) {
      return 0;
		}

	  if (__search_exception_tables(eip)) {
      safe = 1;
	  }

		if (insn_has_rep_prefix(insn)) {
			if (insn_has_translation(v->bt, insn)) {
        rep_prefix = true;
			} else {
				return 0;
			}
		}

		opc_size = get_opcode_size(v->bt, insn);

		/* Dead flag logic prevent scas and cmps to save and restore
		 * flags during loop. FIXME:scas change only status flag
		 */
	  check_dead_flags_and_tregs(v, eip, &treg, &opt_treg, 
				&dead_flags, rep_prefix, insn, (opc_size==4)?NUM_REGS:4);

		ASSERT(treg != -1);
		ASSERT(opt_treg == -1 || opt_treg == treg);

		if (!dead_flags) {
      peepgen_emit_code(v, optr, eptr, &is_term, 
					peep_snippet_pushfl, 0);
		}

		if (opt_treg == -1) {
      peepgen_emit_code(v, optr, eptr, &is_term, 
					peep_snippet_push_treg, 1, treg);
		}

		if (!rep_prefix) {
      gen_shadow_code(v, optr, eptr, insn, op1, safe, treg, 
					opc_size, eip, &is_term);
			ASSERT(op2 == NULL);
		} else {
      gen_shadow_str_code(v, optr, eptr, insn, safe, 
					treg, opc_size, eip, &is_term);
		}

		if (opt_treg == -1) {
      peepgen_emit_code(v, optr, eptr, &is_term,
					peep_snippet_pop_treg, 1, treg);
		}
		if (!dead_flags) {
      peepgen_emit_code(v, optr, eptr, &is_term, 
					peep_snippet_popfl, 0);
		}
	}
  return rep_prefix;
}

void
count_insns(btmod_vcpu *v, uint32_t eip_virt, 
		uint8_t **optr, uint8_t **eptr, int num_insns)
{
  if (shadow_idt_loaded) {
    bool flags_are_dead = check_if_flags_are_dead(v, eip_virt);
	  bool is_terminating;
    if (flags_are_dead) {
      peepgen_emit_code(v, optr, eptr, &is_terminating, 
				peep_snippet_inc_sreg0f, 1, num_insns);
    } else {
      peepgen_emit_code(v, optr, eptr, &is_terminating, 
				peep_snippet_inc_sreg0, 1, num_insns);
    }
  }
}
