#ifndef PEEP_I386_DIS_H
#define PEEP_I386_DIS_H
#include <types.h>
#include <linux/types.h>

struct btmod;
struct insn_t;
struct btmod_vcpu;

#define MAX_OPERANDS 4

typedef unsigned char bfd_byte;
typedef unsigned long bfd_vma;
typedef long bfd_signed_vma;

typedef struct bti386_t {
  int prefixes;
	int rex;
	int rex_used;
	int used_prefixes;
	char obuf[100];
	char *obufp;
	char scratchbuf[100];
	char const *rename_opcode;
	unsigned char const *start_codep;
	unsigned char const *insn_codep;
	unsigned char const *codep;
	unsigned char need_modrm;
	char op_out[MAX_OPERANDS][100];
	int op_ad, op_index[MAX_OPERANDS];
  int two_source_ops;
	bfd_vma op_address[MAX_OPERANDS];
	bfd_vma op_riprel[MAX_OPERANDS];
	bfd_vma start_pc;
	char intel_syntax;
	char open_char;
	char close_char;
	char separator_char;
	char scale_char;
	struct {
	  int mod;
		int reg;
		int rm;
	} modrm;
} bti386_t;

void opc_init(struct btmod_vcpu *v);
int sprint_insn(bti386_t *bti386, unsigned long pc, char *buf, size_t buf_size, unsigned size,
		bool guest);
int disas_insn(struct btmod_vcpu *v, unsigned char const *code, target_ulong eip,
    struct insn_t *insn, unsigned size, bool guest);

#endif
