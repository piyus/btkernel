#ifndef PEEP_INSN_H
#define PEEP_INSN_H
#include <linux/types.h>
#include "peep/insntypes.h"
#include "bt_vcpu.h"

struct insn_t;
struct regset_t;
struct assignments_t;

int insn2str(btmod *bt, struct insn_t const *insn, char *buf, size_t size);
int insns2str(btmod *bt, struct insn_t const *insns, int n_insns, char *buf, size_t size);
int operand2str(operand_t const *op, char *buf, size_t size);
bool str2operand(operand_t *op, char const *str);
void insn_init(struct insn_t *insn);
void insn_get_usedef(struct insn_t const *insn, struct regset_t *use,
    struct regset_t *def);
void __insn_get_usedef(btmod *bt, struct insn_t const *insn, 
		struct regset_t *use, struct regset_t *def);
bool insn_is_terminating(btmod *bt, struct insn_t const *insn);
bool insn_accesses_mem16(btmod *bt, struct insn_t const *insn, struct operand_t const **op1,
		struct operand_t const **op2);
bool insn_accesses_mem(btmod *bt, struct insn_t const *insn, struct operand_t const **op1,
		struct operand_t const **op2);
bool insn_accesses_stack(btmod *bt, insn_t const *insn);
bool insn_is_prefetch(btmod *bt, insn_t const *insn);
struct operand_t const *insn_has_prefix(btmod *bt, insn_t const *insn);
size_t insn_get_operand_size(insn_t const *insn);
size_t insn_get_addr_size(btmod *bt, insn_t const *insn);
bool insn_is_conditional_jump(btmod *bt, insn_t const *insn);
bool insn_is_direct_jump(btmod *bt, insn_t const *insn);
bool insn_is_indirect_jump(btmod *bt, insn_t const *insn);
bool insn_is_string_op(btmod *bt, insn_t const *insn);
bool insn_is_movs_or_cmps(btmod *bt, insn_t const *insn);
bool insn_is_cmps_or_scas(btmod *bt, insn_t const *insn);
bool insn_is_push(btmod *bt, insn_t const *insn);
bool insn_is_pop(btmod *bt, insn_t const *insn);
bool insn_is_sti(btmod *bt, insn_t const *insn);
void insn_rename_constants(insn_t *insn,
		struct assignments_t const *assignments);

long operand_get_value(operand_t const *op);
opertype_t str2tag(char const *tagstr);
bool insn_reads_flags(btmod *bt, struct insn_t const *insn);
bool insn_sets_flags(btmod *bt, struct insn_t const *insn);
void print_insn(btmod *bt, insn_t const *insn);
bool insn_has_rep_prefix(insn_t const *insn);
int get_opcode_size(btmod *bt, insn_t const *insn);
bool insn_can_do_exception(btmod *bt, insn_t const *insn);
bool writes_operand(btmod *bt, insn_t const *insn, const operand_t *op);
bool insn_has_indirect_dependence(btmod *bt, insn_t const *insn);
bool insn_has_direct_dependence(btmod *bt, insn_t const *insn);
bool insn_is_direct_call(btmod *bt, insn_t const *insn);
bool insn_has_translation(btmod *bt, insn_t const *insn);

#endif /* peep/insn.h */
