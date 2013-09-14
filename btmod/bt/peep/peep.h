#ifndef PEEP_PEEP_H
#define PEEP_PEEP_H
#include <linux/types.h>
#include <types.h>
#include "cpu_constraints.h"
#include "assignments.h"
#include "regset.h"

#define PEEP_PREFIX peep_
#define ROLLBACK_PREFIX rb_
#define MAX_PEEP_STRING_SIZE  80
#define MAX_TU_SIZE 2000         /* Max number of insns in a translation unit. */
struct btmod;
struct btmod_vcpu;
struct tb_t;

typedef struct bt_peep_t {
	assignments_t tmp_assignments;
	char templ_buf[64];
	char op_buf[64];
	uint8_t prefixes[16];
	regset_t use, def;
	insn_t insn;
	uint8_t ptr_copy[32];
	char insns_buf[256];
	assignments_t assignments;
	long params[32];
	insn_t insns[MAX_TU_SIZE];
	uint16_t edge_offset[3], jmp_offset[3];
	long params1[4];
} bt_peep_t;

int peep_init(struct btmod *bt);
void peep_uninit(struct btmod *bt);
void translate(struct btmod_vcpu *v, struct tb_t *tb, bool tmode);

#endif
