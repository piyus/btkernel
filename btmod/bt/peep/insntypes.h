#ifndef __INSNTYPES_H
#define __INSNTYPES_H

#include <linux/types.h>
#include "peep/opctable.h"

#define NUM_REGS 8
#define NUM_SEGS 6
#define NUM_CRS  5
#define MAX_OPERAND_STRSIZE 16

/* Flags stored in PREFIXES.  */
#define PREFIX_REPZ 1
#define PREFIX_REPNZ 2
#define PREFIX_LOCK 4
#define PREFIX_CS 8
#define PREFIX_SS 0x10
#define PREFIX_DS 0x20
#define PREFIX_ES 0x40
#define PREFIX_FS 0x80
#define PREFIX_GS 0x100
#define PREFIX_DATA 0x200
#define PREFIX_ADDR 0x400
#define PREFIX_FWAIT 0x800

#define MAX_TEMPORARIES 8

#define PREFIX_BYTE_SEG(i) (((i) == R_ES)?0x26:((i) == R_CS)?0x2e:((i) == R_SS)?0x36:((i) == R_DS)?0x3e:((i) == R_FS)?0x64:((i) == R_GS)?0x65:-1)


typedef enum {
  invalid = 0,
  op_reg,
  op_seg,
  op_mem,
  op_imm,
	op_pcrel,
  op_cr,
  op_db,
  op_tr,
  op_mmx,
  op_xmm,
  op_3dnow,
  op_prefix,
	op_st,
} opertype_t;

typedef enum {
  tag_const = 0,
  tag_var,
  tag_eax,
  tag_abcd,             /* If for an op_reg arg, only e[abcd]x are allowed. */
  tag_no_esp,
  tag_no_eax,
  tag_no_eax_esp,
  tag_no_cs_gs,
  tag_cs_gs,
} tag_t;

typedef enum {
  segtype_sel = 0,
  segtype_desc
} segtype_t;

typedef struct {
  unsigned addrsize:4;        //in bytes
  unsigned segtype:2;
  union {
    unsigned all:16;
    int sel:4;
    unsigned desc:16;
  } seg;
  int base:5;
  unsigned scale:4;
  int index:5;
  uint64_t disp;
} operand_mem_val_t;

typedef struct {
  unsigned all:4;                    /* colocated with tag->all*/
  union {
    unsigned all:4;
    unsigned sel:4;
    unsigned desc:4;
  } seg;
  unsigned base:4;
  unsigned index:4;
  unsigned disp:4;
} operand_mem_tag_t;

typedef struct operand_t {
  opertype_t type;
  unsigned size:4;    //in bytes
  unsigned rex_used:1;

  /* Hold either variables or constants. */

  union {
    operand_mem_val_t mem;
    unsigned seg:3;
    unsigned reg:4;
    uint64_t imm;
		uint64_t pcrel;
    unsigned cr:3;
    unsigned db:4;
    unsigned tr:3;
    unsigned mmx:3;
    unsigned xmm:3;
    uint8_t d3now;
    unsigned prefix:3;
		unsigned st:3;
  } val;

  union {
    unsigned all:4;
    operand_mem_tag_t mem;
    unsigned seg:4;
    unsigned reg:4;
    unsigned imm:4;
		unsigned pcrel:4;
    unsigned cr:4;
    unsigned db:4;
    unsigned tr:4;
    unsigned mmx:4;
    unsigned xmm:4;
    unsigned d3now:4;
    unsigned prefix:4;
		unsigned st:4;
  } tag;
} operand_t;


typedef struct insn_t {
  opc_t opc;
  char opcstr[8];       /* only for better readability of peepgen_entries.h
                           not used anywhere else. */
  operand_t op[3];
} insn_t;

struct btmod;
//void print_insn(insn_t const *insn);
void println_insn(struct btmod *bt, insn_t const *insn);

/* Operand functions. */
bool operands_equal(operand_t const *op1, operand_t const *op2);
bool str2operand(operand_t *op, char const *str);
char const *get_opertype_str(opertype_t type);
char const *tag2str(opertype_t tag);
opertype_t str2tag(char const *tagstr);
#endif
