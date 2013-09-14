#include "peep/peep.h"
#include <linux/types.h>
#include <utils.h>
#include <linux/string.h>
#include <debug.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/smp.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
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
#include "hypercall.h"
#include "btfix.h"
#include "peep/instrument.h"

#ifdef __PEEPGEN__
unsigned long tx_target;
#else
#include <asm/processor.h>
#endif

/* For debugging purposes. */

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

char const *peep_label_str[] = {
  "null",
#define DEF(x) #x,
#include "peepgen_defs.h"
};

peep_entry_t peep_tab_entries[] = {
#include "peepgen_entries.h"
};

#include "gen_snippets.h"

static inline unsigned
hash_insns(size_t n_insns, insn_t const *insns)
{
  unsigned ret;
  unsigned i, j;

  ret = n_insns*313;
  for (i = 0; i < n_insns; i++) {
    ret += insns[i].opc*1601;
    for (j = 0; j < 3; j++) {
      ret += ((int)insns[i].op[j].type)*487;
    }
  }
  return ret;
}

static unsigned
peep_entry_hash(struct hash_elem const *elem, void *aux)
{
  struct peep_entry_t const *entry;

  entry = hash_entry(elem, struct peep_entry_t, peeptab_elem);
  return hash_insns(entry->n_tmpl, entry->tmpl);
}

static bool
peep_entry_equal(struct hash_elem const *ea, struct hash_elem const *eb,
    void *aux)
{
  /* We always insert unique entries. */
  return false;
}

/* Matching functions. */

/* macros to be used for disp and seg.desc fields of memory operands. */
#define mem_match(templ, op, memfield, fmem) do {                             \
  if (templ->tag.mem.memfield == tag_const) {                                 \
    if (op->val.mem.memfield != templ->val.mem.memfield) {                    \
      DBE(MATCH, dprintk("      const " #memfield " mismatch.\n"));        \
      return false;                                                           \
    }                                                                         \
  } else {                                                                    \
    operand_t _op, _templ, tmp_op;                                            \
                                                                              \
    _op.type = _templ.type = op_##fmem;                                       \
    _op.size = _templ.size = (_op.type == op_imm)?0:op->val.mem.addrsize;     \
                                                                              \
    _op.val.fmem = op->val.mem.memfield;                                      \
    _op.tag.fmem = op->tag.mem.memfield;                                      \
    _templ.val.fmem = templ->val.mem.memfield;                                \
    _templ.tag.fmem = templ->tag.mem.memfield;                                \
                                                                              \
    if (   _op.type == op_reg                                                 \
        && (uint16_t)op->val.mem.memfield == (uint16_t)-1) {                  \
      return false;                                                           \
    }                                                                         \
                                                                              \
    if (assignments_getval(assignments, &_templ, &tmp_op)) {                  \
      if (!operands_equal(&tmp_op, &_op)) {                                   \
        DBE(MATCH, dprintk("      " #memfield " var!=const mismatch\n"));  \
        return false;                                                         \
      }                                                                       \
    } else {                                                                  \
      if (!assignment_is_coherent(&_templ, &_op)) {                           \
        DBE(MATCH, dprintk("      " #memfield " constraints not "          \
              "satisfied.\n"));                                               \
        return false;                                                         \
      }                                                                       \
      assignments_add(tmp_assignments, &_templ, &_op);                       \
    }                                                                         \
  }                                                                           \
} while (0)

static bool
mem_operands_match(btmod_vcpu *v, operand_t const *op, operand_t const *templ,
    assignments_t *assignments)
{
  assignments_t *tmp_assignments = &v->bt_peep.tmp_assignments;

  ASSERT(op->type == op_mem);
  ASSERT(templ->type == op_mem);
  ASSERT(templ->tag.all == tag_const);

  if (op->val.mem.addrsize != templ->val.mem.addrsize) {
    DBE(MATCH, dprintk("    mem operands : addrsize mismatch.\n"));
    return false;
  }
  if (op->val.mem.segtype != templ->val.mem.segtype) {
    DBE(MATCH, dprintk("    mem operands : segtype mismatch.\n"));
    return false;
  }

  assignments_init(tmp_assignments);

  if (op->val.mem.segtype == segtype_sel) {
    mem_match(templ, op, seg.sel, seg);
  } else {
    mem_match(templ, op, seg.desc, imm);
  }

  mem_match(templ, op, base, reg);
  mem_match(templ, op, index, reg);
  if (templ->val.mem.scale != op->val.mem.scale) {
    return false;
  }
  mem_match(templ, op, disp, imm);

  append_assignments(assignments, tmp_assignments);
  return true;
}

static bool
operands_match(btmod_vcpu *v, operand_t const *op, operand_t const *templ,
  assignments_t *assignments)
{
  if (op->type != templ->type) {
    DBE(MATCH, dprintk("    type mismatch (%d <=> %d).\n", op->type,
            templ->type));
    return false;
  }
  if (op->size != templ->size) {
    DBE(MATCH, dprintk("    op->size != templ->size.\n"));
    if (op->size != 0) {
      DBE(MATCH, dprintk("    size mismatch (%d <=> %d).\n", op->size,
              templ->size));
      return false;
    }
    /* op->size == 0 means that the constant is size-agnostic, while the
     * variable might be size-sensitive.
     */
    switch (op->type) {
      case op_imm:
        if (op->val.imm < (1ULL << (templ->size*8))) {
          assignments_add(assignments, templ, op);
          return true;
        } else {
          DBE(MATCH, dprintk("    imm size mismatch (templ size %d, "
                  "constant %llu).\n", templ->size, op->val.imm));
          return false;
        }
      default:
        dprintk("\nop->size = %d. templ->size = %d. op->type = %d\n",
            op->size, templ->size, op->type);
        ASSERT(0);
    }
  }
  if (op->type == invalid) {
    return true;
  }

  if (templ->tag.all == tag_const) {
    if (op->type != op_mem) {
      if (!operands_equal(templ, op)) {
        bt_peep_t *p = &v->bt_peep;
        operand2str(templ, p->templ_buf, sizeof p->templ_buf);
        operand2str(op, p->op_buf, sizeof p->op_buf);
        DBE(MATCH, dprintk("    cons operands mismatch (%s <=> %s).\n",
              p->templ_buf, p->op_buf));
        return false;
      }
      return true;
    } else {
      DBE(MATCH, dprintk("    calling mem_operands_match().\n"));
      if (!mem_operands_match(v, op, templ, assignments)) {
        DBE(MATCH, dprintk("    mem operands mismatch.\n"));
        return false;
      }
      DBE(MATCH, dprintk("    mem_operands_match() returned true.\n"));
      return true;
    }
  } else {
    operand_t tmp_op;

    if (assignments_getval(assignments, templ, &tmp_op)) {
      if (!operands_equal(&tmp_op, op)) {
        DBE(MATCH, dprintk("    template mismatch for variable value.\n"));
        return false;
      }
    } else {
      if (assignment_is_coherent(templ, op)) {
        assignments_add(assignments, templ, op);
        return true;
      } else {
        DBE(MATCH, dprintk("    constant not coherent with variable.\n"));
        return false;
      }
    }
  }
	NOT_REACHED();
  return false;
}

static bool
templ_matches_insn(btmod_vcpu *v, insn_t *insn, insn_t *tmpl, assignments_t *assignments)
{
  int i;

  if (insn->opc != tmpl->opc) {
    DBE(MATCH_ALL, dprintk("    opcodes mismatch (%s <=> %s).\n",
          opctable_name(v->bt, insn->opc), opctable_name(v->bt, tmpl->opc)));
    return false;
  }

  DBE(MATCH, dprintk("    opcodes match. checking operands.\n"));
  for (i = 0; i < 3; i++) {
    if (!operands_match(v, &insn->op[i], &tmpl->op[i], assignments)) {
      DBE(MATCH, dprintk("    operand #%d mismatch.\n", i));
      return false;
    }
  }
  return true;
}

static bool
templ_matches_insns(btmod_vcpu *v, insn_t *insns, int n_insns, insn_t *templ, int n_templ,
    assignments_t *assignments)
{
  int i;
  assignments_init(assignments);     /* start with a clean assignments array. */

  if (n_insns != n_templ) {
    DBE(MATCH_ALL, dprintk("    n_insns != n_templ.\n"));
    return false;
  }
  for (i = 0; i < n_insns; i++) {
    if (!templ_matches_insn(v, &insns[i], &templ[i], assignments)) {
      DBE(MATCH_ALL, dprintk("    failed on insn #%d\n", i));
      return false;
    }
  }
  return true;
}

static bool
temp_assignment_violates_nomatch_pairs(int tempno, int reg,
    assignments_t const *assignments, nomatch_pair_t const *nomatch_pairs,
    int num_nomatch_pairs)
{
  int reg_indices[assignments->num_assignments];
  int i, num_regs, n;

  for (n = 0; n < num_nomatch_pairs; n++) {
    int otherval = -1;
    tag_t othertag = tag_const;
    if (   nomatch_pairs[n].reg1 == TEMP_REG0 + tempno
        && nomatch_pairs[n].tag1 == tag_var) {
      otherval = nomatch_pairs[n].reg2;
      othertag = nomatch_pairs[n].tag2;
    }
    if (   nomatch_pairs[n].reg2 == TEMP_REG0 + tempno
        && nomatch_pairs[n].tag2 == tag_var) {
      otherval = nomatch_pairs[n].reg1;
      othertag = nomatch_pairs[n].tag1;
    }
    if (otherval == -1) {
      continue;
    }
    if (othertag == tag_const) {
      if (otherval == reg) {
        return true;
      }
    } else {
      if (otherval < TEMP_REG0) { /* ignore temps. they are checked earlier. */
        num_regs = assignments_get_regs(reg_indices, assignments);
        for (i = 0; i < num_regs; i++) {
          int r = reg_indices[i];
          ASSERT(assignments->arr[r].var.type == op_reg);
          ASSERT(assignments->arr[r].val.type == op_reg);
          ASSERT(assignments->arr[r].var.tag.reg != tag_const);
          ASSERT(assignments->arr[r].val.tag.reg == tag_const);
          ASSERT(assignments->arr[r].var.val.reg < TEMP_REG0);
          if (otherval == assignments->arr[r].var.val.reg) {
            if (reg == assignments->arr[r].val.val.reg) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

static bool
find_temporary_regs(int *temporaries, int n_temporaries,
    tag_t const *temporary_tags, assignments_t const *assignments,
    nomatch_pair_t const *nomatch_pairs, int num_nomatch_pairs)
{
  char temp_regs[NUM_REGS];
  int j;
  memset(temp_regs, 0x0, sizeof temp_regs);
  for (j = 0; j < n_temporaries; j++) {
    int r;
    for (r = NUM_REGS - 1; r >= 0; r--) {
      operand_t opcons = {op_reg, 4, 0, {.reg=r}, {.reg=tag_const}};
      operand_t opvar = {op_reg, 4, 0, {.reg=0},
        {.reg=temporary_tags?temporary_tags[j]:tag_var}};
      if (   assignment_is_coherent(&opvar, &opcons)
          && !temp_regs[r]
          && !temp_assignment_violates_nomatch_pairs(j, r, assignments,
            nomatch_pairs, num_nomatch_pairs)) {
        temp_regs[r] = 1;
        break;
      }
    }
    if (r < 0) {
      int i;
      for (i = 0; i < NUM_REGS; i++) {
        operand_t opcons = {op_reg, 4, 0, {.reg=i}, {.reg=tag_const}};
        operand_t opvar = {op_reg, 4, 0, {.reg=0},
          {.reg=temporary_tags[j]}};
				(void)opcons;
				(void)opvar;
        dprintk("    reg %d: %s %s %s\n", i, temp_regs[i]?"temp":"",
            assignment_is_coherent(&opvar, &opcons)?"":"not-coherent",
            temp_assignment_violates_nomatch_pairs(j, i, assignments,
              nomatch_pairs, num_nomatch_pairs)?"violates-nomatch-pairs":"");
      }
      return false;
    }
    ASSERT(j < MAX_TEMPORARIES);
    ASSERT(r >= 0 && r < NUM_REGS);
    temporaries[j] = r;
  }
  return true;
}

static void
mode_translate(uint8_t **out_buf, size_t out_buf_size, 
		void *in_buf, size_t in_buf_len)
{
  char const *ptr = (char const *)in_buf;
  uint8_t *obuf = *out_buf;
  size_t i;

  ASSERT(in_buf_len <= out_buf_size);
  for (i = 0; i < in_buf_len; i++) {
	  *obuf++ = ldub((target_ulong)ptr);
		ptr++;
	}
  *out_buf = obuf;
}

void *
hw_memcpy(void *dst, const void *src, size_t n)
{
  void *ret;
  ret = memcpy(dst, src, n);
  return ret;
}

#define PEEP_STRING_SIZE 80

static size_t
peep_translate(btmod_vcpu *v, uint8_t **buf, uint8_t **ebuf, 
		insn_t *insns, int n_insns, target_ulong fallthrough_addr,
    bool *is_terminating)
{
  /* static-allocate all array types. */
  bt_peep_t *p = &v->bt_peep;
  int label;
  int n_params = 0;
  struct mylist *eqlist;
  struct mylist_elem *e;
	cpu_constraints_t constraints;

  DBE(MATCH_ALL, insns2str(v->bt, insns, n_insns, p->insns_buf, sizeof p->insns_buf));
  DBE(MATCH_ALL, dprintk("\nTranslating %s:\n", p->insns_buf));

	constraints = CPU_CONSTRAINT_NO_EXCP | CPU_CONSTRAINT_PROTECTED;
  eqlist = hash_find_bucket_with_hash(&v->bt->peep_tab, hash_insns(n_insns, insns));
  for (e = mylist_begin(eqlist); e != mylist_end(eqlist); e = mylist_next(e)) {
    struct peep_entry_t *entry;
    struct hash_elem *elem;
    /* convert mylist_elem to hash_elem. */
    elem = mylist_entry(e, struct hash_elem, mylist_elem);
    /* convert hash_elem to peep_entry_t. */
    entry = hash_entry(elem, peep_entry_t, peeptab_elem);
    DBE(MATCH_ALL, dprintk("  checking with %s:\n", entry->name));
    if ((constraints & entry->cpu_constraints) != constraints) {
      DBE(MATCH_ALL, dprintk("  cpu_constraints mismatch. %#llx is not "
            "contained in %#llx\n", constraints, entry->cpu_constraints));
      continue;
    }
    if (templ_matches_insns(v, insns, n_insns, entry->tmpl, entry->n_tmpl,
          &p->assignments)) {
      int temporaries[entry->n_temporaries];
      unsigned j, num_reg_params = 0;

      DBE(MATCH, dprintk("  MATCHED!\n"));
      label = entry->label;

      for (j = 0; (int)j < p->assignments.num_assignments; j++) {
        p->params[n_params++] = operand_get_value(&p->assignments.arr[j].val);
        if (p->assignments.arr[j].var.type == op_reg) {
          ASSERT(p->assignments.arr[j].var.type == p->assignments.arr[j].val.type);
          ASSERT(p->params[n_params-1] == p->assignments.arr[j].val.val.reg);
          num_reg_params++;
        }
      }

      if (find_temporary_regs(temporaries, entry->n_temporaries,
            entry->temporaries, &p->assignments,
            entry->nomatch_pairs, entry->num_nomatch_pairs) == false) {
        ERR("Too many registers used in peephole rule %s [num_reg_params=%d, "
            "num_temporaries=%d].\n", peep_label_str[(size_t)label],
            num_reg_params, entry->n_temporaries);
      }
      for (j = 0; j < entry->n_temporaries; j++) {
        ASSERT(temporaries[j] >= 0 && temporaries[j] < NUM_REGS);
        p->params[n_params++] = temporaries[j];
      }

      peepgen_code(v, label, p->params, buf, ebuf,
        &tx_target, fallthrough_addr, is_terminating);
      return 1;
    }
    DBE(MATCH, dprintk("  not matched. hash=%#x!\n",
          peep_entry_hash(&entry->peeptab_elem, NULL)));
  }
  return 0;
}

void
translate(btmod_vcpu *v, tb_t *tb, bool tmode)
{
	btmod *bt = v->bt;
  bt_peep_t *p = &v->bt_peep;
	const struct exception_table_entry *e;
  uint8_t *code, *ptr, *ptr_next;
  uint8_t *optr, *oend, *eptr, *eend;
  int n_in = 0, peep, disas, tpage_size, epage_size;
  target_ulong cur_addr, fallthrough_addr, eip_virt;
  bool is_terminating;
	bool code_generated = false;

  eip_virt = tb->eip_virt;
	code = ptr = (uint8_t *)eip_virt;

	tpage_size = (tmode)?tb->code_len:(PAGE_SIZE * 16);
	epage_size = (tmode)?tb->edge_len:(PAGE_SIZE * 16);

  optr = (tmode)?tb->tc_ptr:bt->tpage;
  oend = optr + tpage_size;
  eptr = (tmode)?tb->edge_ptr:bt->tpage;
  eend = eptr + epage_size;

#if defined(INSCOUNT) || defined(PROFILING)
	count_insns(v, eip_virt, &optr, &eptr, tb->num_insns);
#endif

  do {
    if (tmode) {
      tb->tc_boundaries[n_in] = optr - (uint8_t*)tb->tc_ptr;
    }

    cur_addr = (ptr - code) + eip_virt;
    disas = disas_insn(v, ptr, cur_addr, &p->insns[n_in], 4, true);
    ASSERT(disas);

    ptr_next = ptr + disas;
    ASSERT(n_in < MAX_TU_SIZE);

    fallthrough_addr = (ptr_next - code) + eip_virt;
    is_terminating = insn_is_terminating(bt, &p->insns[n_in]);

    if (tmode && (e = __search_exception_tables(cur_addr))) {
		  add_bt_exception_table((uint32_t)optr, (uint32_t)e->fixup);
		}

		if (shadow_idt_loaded && _shadowmem_start > 0) {
		  code_generated = instrument_memory(v, cur_addr,
			    &p->insns[n_in], &optr, &eptr, tmode);
		}
		ASSERT(optr <= oend && eptr <= eend);

    peep = peep_translate(v, &optr, &eptr, &p->insns[n_in], 1,
		  fallthrough_addr, &is_terminating);

    ASSERT(optr <= oend && eptr <= eend);
		is_terminating = peep && is_terminating;

    if (!peep && !code_generated) {
      mode_translate(&optr, oend - optr, ptr, ptr_next - ptr);
    }
    ASSERT(optr <= oend && eptr <= eend);

    ptr = ptr_next;
    if (tmode) {
      tb->eip_boundaries[n_in] = ptr - code;
    }
    n_in++;
  } while (!is_terminating);

  tb->end_eip = cur_addr;
	if (!tmode) {
    tb->num_insns = n_in;
    tb->code_len = optr - (uint8_t*)bt->tpage;
    tb->edge_len = eptr - (uint8_t*)bt->tpage;
	} else {
    tb->tc_boundaries[n_in] = optr - (uint8_t*)tb->tc_ptr;
  }
}

int
peep_init(btmod *bt)
{
  unsigned i;
  bool ret;
  size_t peeptab_size = sizeof peep_tab_entries/sizeof peep_tab_entries[0];
  ret = hash_init_size(&bt->peep_tab, max(256U, peeptab_size), peep_entry_hash,
      peep_entry_equal, NULL);
  ASSERT(ret);
  if (!ret) {
    return 0;
  }
  for (i = 0; i < peeptab_size; i++) {
    hash_insert(&bt->peep_tab, &peep_tab_entries[i].peeptab_elem);
  }
	return 1;
}

void
peep_uninit(btmod *bt)
{
  struct mylist *buckets;
	int bucket_cnt;

  buckets = bt->peep_tab.buckets;
	bucket_cnt = bt->peep_tab.bucket_cnt;
  btfree(buckets, sizeof(*buckets)*bucket_cnt);
}
