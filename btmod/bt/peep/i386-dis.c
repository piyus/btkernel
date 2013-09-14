/* Print i386 instructions for GDB, the GNU debugger.
	 Copyright 1988, 1989, 1991, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
	 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

	 This file is part of the GNU opcodes library.

	 This library is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation; either version 3, or (at your option)
	 any later version.

	 It is distributed in the hope that it will be useful, but WITHOUT
	 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
	 License for more details.

	 You should have received a copy of the GNU General Public License
	 along with this program; if not, write to the Free Software
	 Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
	 MA 02110-1301, USA.  */


/* 80386 instruction printer by Pace Willisson (pace@prep.ai.mit.edu)
	 July 1988
	 modified by John Hassey (hassey@dg-rtp.dg.com)
	 x86-64 support added by Jan Hubicka (jh@suse.cz)
	 VIA PadLock support by Michal Ludvig (mludvig@suse.cz).  */

/* The main tables describing the instructions is essentially a copy
	 of the "Opcode Map" chapter (Appendix A) of the Intel 80386
	 Programmers Manual.  Usually, there is a capital letter, followed
	 by a small letter.  The capital letter tell the addressing mode,
	 and the small letter tells about the operand size.  Refer to
	 the Intel manual for details.  */
#include <types.h>
#include "peep/i386-dis.h"
#include <linux/types.h>
#include <linux/string.h>
#include <debug.h>
#include "peep/insntypes.h"
#include "peep/opctable.h"
#include "sys/vcpu.h"
#include "peep/insn.h"
#include <linux/kernel.h>
#include <lib/utils.h>
#include "hypercall.h"

#define MAX_NUM_OPERANDS 3

#define _(x) x
#define ATTRIBUTE_UNUSED __attribute__ ((__unused__))

#define CONST_STRNEQ(STR1,STR2) (strncmp ((STR1), (STR2), sizeof (STR2) - 1) == 0)

#ifndef SYSV386_COMPAT
/* Set non-zero for broken, compatible instructions.  Set to zero for
	 non-broken opcodes at your peril.  gcc generates SystemV/386
	 compatible instructions.  */
#define SYSV386_COMPAT 1
#endif
#ifndef OLDGCC_COMPAT
/* Set non-zero to cater for old (<= 2.8.1) versions of gcc that could
	 generate nonsense fsubp, fsubrp, fdivp and fdivrp with operands
	 reversed.  */
#define OLDGCC_COMPAT SYSV386_COMPAT
#endif

#define MOV_AX_DISP32 0xa0
#define POP_SEG_SHORT 0x07
#define JUMP_PC_RELATIVE 0xeb
#define INT_OPCODE  0xcd
#define INT3_OPCODE 0xcc
/* The opcode for the fwait instruction, which disassembler treats as a
	 prefix when it can.  */
#define FWAIT_OPCODE 0x9b
#define ADDR_PREFIX_OPCODE 0x67
#define DATA_PREFIX_OPCODE 0x66
#define LOCK_PREFIX_OPCODE 0xf0
#define CS_PREFIX_OPCODE 0x2e
#define DS_PREFIX_OPCODE 0x3e
#define ES_PREFIX_OPCODE 0x26
#define FS_PREFIX_OPCODE 0x64
#define GS_PREFIX_OPCODE 0x65
#define SS_PREFIX_OPCODE 0x36
#define REPNE_PREFIX_OPCODE 0xf2
#define REPE_PREFIX_OPCODE  0xf3

#define TWO_BYTE_OPCODE_ESCAPE 0x0f
#define NOP_OPCODE (char) 0x90

/* register numbers */
#define EBP_REG_NUM 5
#define ESP_REG_NUM 4

/* modrm_byte.regmem for twobyte escape */
#define ESCAPE_TO_TWO_BYTE_ADDRESSING ESP_REG_NUM
/* index_base_byte.index for no index register addressing */
#define NO_INDEX_REGISTER ESP_REG_NUM
/* index_base_byte.base for no base register addressing */
#define NO_BASE_REGISTER EBP_REG_NUM
#define NO_BASE_REGISTER_16 6

/* modrm.mode = REGMEM_FIELD_HAS_REG when a register is in there */
#define REGMEM_FIELD_HAS_REG 0x3/* always = 0x3 */
#define REGMEM_FIELD_HAS_MEM (~REGMEM_FIELD_HAS_REG)

/* x86-64 extension prefix.  */
#define REX_OPCODE	0x40

/* Indicates 64 bit operand size.  */
#define REX_W	8
/* High extension to reg field of modrm byte.  */
#define REX_R	4
/* High extension to SIB index field.  */
#define REX_X	2
/* High extension to base field of modrm or SIB, or reg field of opcode.  */
#define REX_B	1

/* max operands per insn */
#define MAX_OPERANDS 4

/* max immediates per insn (lcall, ljmp, insertq, extrq) */
#define MAX_IMMEDIATE_OPERANDS 2

/* max memory refs per insn (string ops) */
#define MAX_MEMORY_OPERANDS 2

/* max size of insn mnemonics.  */
#define MAX_MNEM_SIZE 16

/* max size of register name in insn mnemonics.  */
#define MAX_REG_NAME_SIZE 8

#undef ldub
#define ldub(ptr) (*((uint8_t*)(ptr)))

#define dis_ldub(ptr) ({																											\
		uint8_t ret;																																\
		if (guest_flag) {																														\
		ret = ldub((target_ulong)ptr);																						\
		} else {																																		\
		ret = *(uint8_t *)(ptr);																									\
		}																																						\
		ret;																																				\
		})
bool guest_flag = false;

static void ckprefix (bti386_t *bti386);
static const char *prefix_name (bti386_t *bti386, int, int);
static void dofloat (bti386_t *bti386, int);
static void disas_dofloat (btmod_vcpu *v, insn_t *insn, int);
static void OP_ST (bti386_t *bti386, int, int);
static void OP_STi (bti386_t *bti386, int, int);
static int putop (bti386_t *bti386, const char *, int);
static void oappend (bti386_t *bti386, const char *);
static void append_seg (bti386_t *bti386);
static void OP_indirE (bti386_t *bti386, int, int);
static void print_operand_value (bti386_t *bti386, char *, int, int, bfd_vma);
static void print_displacement (bti386_t *bti386, char *, bfd_vma, size_t);
static void OP_E (bti386_t *bti386, int, int);
static void OP_G (bti386_t *bti386, int, int);
static bfd_vma get64 (bti386_t *bti386);
static bfd_signed_vma get32 (bti386_t *bti386);
static bfd_signed_vma get32s (bti386_t *bti386);
static int get16 (bti386_t *bti386);
static void set_op (bti386_t *bti386, bfd_vma, int);
static void OP_REG (bti386_t *bti386, int, int);
static void OP_IMREG (bti386_t *bti386, int, int);
static void OP_I (bti386_t *bti386, int, int);
static void OP_I64 (bti386_t *bti386, int, int);
static void OP_sI (bti386_t *bti386, int, int);
static void OP_J (bti386_t *bti386, int, int);
static void OP_SEG (bti386_t *bti386, int, int);
static void OP_DIR (bti386_t *bti386, int, int);
static void OP_OFF (bti386_t *bti386, int, int);
static void OP_OFF64 (bti386_t *bti386, int, int);
static void ptr_reg (bti386_t *bti386, int, int);
static void OP_ESreg (bti386_t *bti386, int, int);
static void OP_DSreg (bti386_t *bti386, int, int);
static void OP_C (bti386_t *bti386, int, int);
static void OP_D (bti386_t *bti386, int, int);
static void OP_T (bti386_t *bti386, int, int);
static void OP_R (bti386_t *bti386, int, int);
static void OP_MMX (bti386_t *bti386, int, int);
static void OP_XMM (bti386_t *bti386, int, int);
static void OP_EM (bti386_t *bti386, int, int);
static void OP_EX (bti386_t *bti386, int, int);
static void OP_EMC (bti386_t *bti386, int,int);
static void OP_MXC (bti386_t *bti386, int,int);
static void OP_MS (bti386_t *bti386, int, int);
static void OP_XS (bti386_t *bti386, int, int);
static void OP_M (bti386_t *bti386, int, int);
static void OP_VMX (bti386_t *bti386, int, int);
static void OP_0fae (bti386_t *bti386, int, int);
static void OP_0f07 (bti386_t *bti386, int, int);
static void NOP_Fixup1 (bti386_t *bti386, int, int);
static void NOP_Fixup2 (bti386_t *bti386, int, int);
static void OP_3DNowSuffix (bti386_t *bti386, int, int);
static void OP_SIMD_Suffix (bti386_t *bti386, int, int);
static void SIMD_Fixup (bti386_t *bti386, int, int);
static void PNI_Fixup (bti386_t *bti386, int, int);
static void SVME_Fixup (bti386_t *bti386, int, int);
static void INVLPG_Fixup (bti386_t *bti386, int, int);
static void BadOp (bti386_t *bti386);
static void VMX_Fixup (bti386_t *bti386, int, int);
static void REP_Fixup (bti386_t *bti386, int, int);
static void Prefix (bti386_t *bti386, int, int);
static void CMPXCHG8B_Fixup (bti386_t *bti386, int, int);
static void XMM_Fixup (bti386_t *bti386, int, int);
static void CRC32_Fixup (bti386_t *bti386, int, int);


static void DIS_ST (bti386_t *bti386, operand_t *, int, int);
static void DIS_STi (bti386_t *bti386, operand_t *, int, int);
static void DIS_indirE (bti386_t *bti386, operand_t *, int, int);
static void DIS_E (bti386_t *bti386, operand_t *, int, int);
static void DIS_G (bti386_t *bti386, operand_t *, int, int);
static void DIS_REG (bti386_t *bti386, operand_t *, int, int);
static void DIS_IMREG (bti386_t *bti386, operand_t *, int, int);
static void DIS_I (bti386_t *bti386, operand_t *, int, int);
static void DIS_I64 (bti386_t *bti386, operand_t *, int, int);
static void DIS_sI (bti386_t *bti386, operand_t *, int, int);
static void DIS_J (bti386_t *bti386, operand_t *, int, int);
static void DIS_SEG (bti386_t *bti386, operand_t *, int, int);
static void DIS_DIR (bti386_t *bti386, operand_t *, int, int);
static void DIS_OFF (bti386_t *bti386, operand_t *, int, int);
static void DIS_OFF64 (bti386_t *bti386, operand_t *, int, int);
static void DIS_ESreg (bti386_t *bti386, operand_t *, int, int);
static void DIS_DSreg (bti386_t *bti386, operand_t *, int, int);
static void DIS_C (bti386_t *bti386, operand_t *, int, int);
static void DIS_D (bti386_t *bti386, operand_t *, int, int);
static void DIS_T (bti386_t *bti386, operand_t *, int, int);
static void DIS_R (bti386_t *bti386, operand_t *, int, int);
static void DIS_MMX (bti386_t *bti386, operand_t *, int, int);
static void DIS_XMM (bti386_t *bti386, operand_t *, int, int);
static void DIS_EM (bti386_t *bti386, operand_t *, int, int);
static void DIS_EX (bti386_t *bti386, operand_t *, int, int);
static void DIS_EMC (bti386_t *bti386, operand_t *, int,int);
static void DIS_MXC (bti386_t *bti386, operand_t *, int,int);
static void DIS_MS (bti386_t *bti386, operand_t *, int, int);
static void DIS_XS (bti386_t *bti386, operand_t *, int, int);
static void DIS_M (bti386_t *bti386, operand_t *, int, int);
static void DIS_VMX (bti386_t *bti386, operand_t *, int, int);
static void DIS_0fae (bti386_t *bti386, operand_t *, int, int);
//static void DIS_0f07 (bti386_t *bti386, operand_t *, int, int);
static void DIS_NOP_Fixup1 (bti386_t *bti386, operand_t *op, int, int);
static void DIS_NOP_Fixup2 (bti386_t *bti386, operand_t *op, int, int);
static void DIS_3DNowSuffix (bti386_t *bti386, operand_t *op, int, int);
static void DIS_SIMD_Suffix (bti386_t *bti386, operand_t *op, int, int);
//static void DIS_SIMD_Fixup (operand_t *op, int, int);
//static void DIS_PNI_Fixup (operand_t *op, int, int);
static void DIS_SVME_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_INVLPG_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_VMX_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_REP_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_Prefix (bti386_t *bti386, operand_t *op, int, int);
static void DIS_CMPXCHG8B_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_XMM_Fixup (bti386_t *bti386, operand_t *op, int, int);
static void DIS_CRC32_Fixup (bti386_t *bti386, operand_t *op, int, int);



struct dis_private {
	/* Points to first byte not fetched.  */
	bfd_byte *max_fetched;
	bfd_byte the_buffer[MAX_MNEM_SIZE];
	bfd_vma insn_start;
	int orig_sizeflag;
	//jmp_buf bailout;
};

enum address_mode
{
	mode_16bit,
	mode_32bit,
	mode_64bit
};

enum address_mode address_mode;

/* Flags for the prefixes for the current instruction.  See below.  */
//static int prefixes;

/* REX prefix the current instruction.  See below.  */
//static int rex;
/* Bits of REX we've already used.  */
//static int rex_used;
/* Mark parts used in the REX prefix.  When we are testing for
	 empty prefix (for 8bit register REX extension), just mask it
	 out.  Otherwise test for REX bit is excuse for existence of REX
	 only in case value is nonzero.  */
#define USED_REX(value)					      \
{							                     \
	if (value)						            \
	{							                  \
		if ((bti386->rex & value))				         \
		bti386->rex_used |= (value) | REX_OPCODE;		\
	}							                  \
	else						                  \
	bti386->rex_used |= REX_OPCODE;				      \
}

/* Flags for prefixes which we somehow handled when printing the
	 current instruction.  */
//static int used_prefixes;


#define XX { NULL, 0 }

#define Eb { OP_E, b_mode, DIS_E }
#define Ev { OP_E, v_mode, DIS_E }
#define Ed { OP_E, d_mode, DIS_E }
#define Edq { OP_E, dq_mode, DIS_E }
#define Edqw { OP_E, dqw_mode, DIS_E }
#define Edqb { OP_E, dqb_mode, DIS_E }
#define Edqd { OP_E, dqd_mode, DIS_E }
#define Eq { OP_E, q_mode, DIS_E }
#define indirEv { OP_indirE, stack_v_mode, DIS_indirE }
#define indirEp { OP_indirE, f_mode, DIS_indirE }
#define stackEv { OP_E, stack_v_mode, DIS_E }
#define Em { OP_E, m_mode, DIS_E }
#define Ew { OP_E, w_mode, DIS_E }
#define M { OP_M, 0, DIS_M }		/* lea, lgdt, etc. */
#define Ma { OP_M, v_mode, DIS_M }
#define Mp { OP_M, f_mode, DIS_M }		/* 32 or 48 bit memory operand for LDS, LES etc */
#define Mq { OP_M, q_mode, DIS_M }
#define Gb { OP_G, b_mode, DIS_G }
#define Gv { OP_G, v_mode, DIS_G }
#define Gd { OP_G, d_mode, DIS_G }
#define Gdq { OP_G, dq_mode, DIS_G }
#define Gm { OP_G, m_mode, DIS_G }
#define Gw { OP_G, w_mode, DIS_G }
#define Rd { OP_R, d_mode, DIS_R }
#define Rm { OP_R, m_mode, DIS_R }
#define Ib { OP_I, b_mode, DIS_I }
#define sIb { OP_sI, b_mode, DIS_sI }	/* sign extened byte */
#define Iv { OP_I, v_mode, DIS_I }
#define Iq { OP_I, q_mode, DIS_I }
#define Iv64 { OP_I64, v_mode, DIS_I64 }
#define Iw { OP_I, w_mode, DIS_I }
#define I1 { OP_I, const_1_mode, DIS_I }
#define Jb { OP_J, b_mode, DIS_J }
#define Jv { OP_J, v_mode, DIS_J }
#define Cm { OP_C, m_mode, DIS_C }
#define Dm { OP_D, m_mode, DIS_D }
#define Td { OP_T, d_mode, DIS_T }

#define RMeAX { OP_REG, eAX_reg, DIS_REG }
#define RMeBX { OP_REG, eBX_reg, DIS_REG }
#define RMeCX { OP_REG, eCX_reg, DIS_REG }
#define RMeDX { OP_REG, eDX_reg, DIS_REG }
#define RMeSP { OP_REG, eSP_reg, DIS_REG }
#define RMeBP { OP_REG, eBP_reg, DIS_REG }
#define RMeSI { OP_REG, eSI_reg, DIS_REG }
#define RMeDI { OP_REG, eDI_reg, DIS_REG }
#define RMrAX { OP_REG, rAX_reg, DIS_REG }
#define RMrBX { OP_REG, rBX_reg, DIS_REG }
#define RMrCX { OP_REG, rCX_reg, DIS_REG }
#define RMrDX { OP_REG, rDX_reg, DIS_REG }
#define RMrSP { OP_REG, rSP_reg, DIS_REG }
#define RMrBP { OP_REG, rBP_reg, DIS_REG }
#define RMrSI { OP_REG, rSI_reg, DIS_REG }
#define RMrDI { OP_REG, rDI_reg, DIS_REG }
#define RMAL { OP_REG, al_reg, DIS_REG }
#define RMAL { OP_REG, al_reg, DIS_REG }
#define RMCL { OP_REG, cl_reg, DIS_REG }
#define RMDL { OP_REG, dl_reg, DIS_REG }
#define RMBL { OP_REG, bl_reg, DIS_REG }
#define RMAH { OP_REG, ah_reg, DIS_REG }
#define RMCH { OP_REG, ch_reg, DIS_REG }
#define RMDH { OP_REG, dh_reg, DIS_REG }
#define RMBH { OP_REG, bh_reg, DIS_REG }
#define RMAX { OP_REG, ax_reg, DIS_REG }
#define RMDX { OP_REG, dx_reg, DIS_REG }

#define eAX { OP_IMREG, eAX_reg, DIS_IMREG }
#define eBX { OP_IMREG, eBX_reg, DIS_IMREG }
#define eCX { OP_IMREG, eCX_reg, DIS_IMREG }
#define eDX { OP_IMREG, eDX_reg, DIS_IMREG }
#define eSP { OP_IMREG, eSP_reg, DIS_IMREG }
#define eBP { OP_IMREG, eBP_reg, DIS_IMREG }
#define eSI { OP_IMREG, eSI_reg, DIS_IMREG }
#define eDI { OP_IMREG, eDI_reg, DIS_IMREG }
#define AL { OP_IMREG, al_reg, DIS_IMREG }
#define CL { OP_IMREG, cl_reg, DIS_IMREG }
#define DL { OP_IMREG, dl_reg, DIS_IMREG }
#define BL { OP_IMREG, bl_reg, DIS_IMREG }
#define AH { OP_IMREG, ah_reg, DIS_IMREG }
#define CH { OP_IMREG, ch_reg, DIS_IMREG }
#define DH { OP_IMREG, dh_reg, DIS_IMREG }
#define BH { OP_IMREG, bh_reg, DIS_IMREG }
#define AX { OP_IMREG, ax_reg, DIS_IMREG }
#define DX { OP_IMREG, dx_reg, DIS_IMREG }
#define zAX { OP_IMREG, z_mode_ax_reg, DIS_IMREG }
#define indirDX { OP_IMREG, indir_dx_reg, DIS_IMREG }

#define Sw { OP_SEG, w_mode, DIS_SEG }
#define Sv { OP_SEG, v_mode, DIS_SEG }
#define Ap { OP_DIR, 0, DIS_DIR }
#define Ob { OP_OFF64, b_mode, DIS_OFF64 }
#define Ov { OP_OFF64, v_mode, DIS_OFF64 }
#define Xb { OP_DSreg, eSI_reg, DIS_DSreg }
#define Xv { OP_DSreg, eSI_reg, DIS_DSreg }
#define Xz { OP_DSreg, eSI_reg, DIS_DSreg }
#define Yb { OP_ESreg, eDI_reg, DIS_ESreg }
#define Yv { OP_ESreg, eDI_reg, DIS_ESreg }
#define DSBX { OP_DSreg, eBX_reg, DIS_DSreg }

#define es { OP_REG, es_reg, DIS_REG }
#define ss { OP_REG, ss_reg, DIS_REG }
#define cs { OP_REG, cs_reg, DIS_REG }
#define ds { OP_REG, ds_reg, DIS_REG }
#define fs { OP_REG, fs_reg, DIS_REG }
#define gs { OP_REG, gs_reg, DIS_REG }

#define MX { OP_MMX, 0, DIS_MMX }
#define XM { OP_XMM, 0, DIS_XMM }
#define EM { OP_EM, v_mode, DIS_EM }
#define EMd { OP_EM, d_mode, DIS_EM }
#define EMx { OP_EM, x_mode, DIS_EM }
#define EXw { OP_EX, w_mode, DIS_EX }
#define EXd { OP_EX, d_mode, DIS_EX }
#define EXq { OP_EX, q_mode, DIS_EX }
#define EXx { OP_EX, x_mode, DIS_EX }
#define MS { OP_MS, v_mode, DIS_MS }
#define XS { OP_XS, v_mode, DIS_XS }
#define EMCq { OP_EMC, q_mode, DIS_EMC }
#define MXC { OP_MXC, 0, DIS_MXC }
#define VM { OP_VMX, q_mode, DIS_VMX }
#define OPSUF { OP_3DNowSuffix, 0, DIS_3DNowSuffix }
#define OPSIMD { OP_SIMD_Suffix, 0, DIS_SIMD_Suffix }
#define XMM0 { XMM_Fixup, 0, DIS_XMM_Fixup }

/* Used handle "rep" prefix for string instructions.  */
#define Xbr { REP_Fixup, eSI_reg, DIS_REP_Fixup }
#define Xvr { REP_Fixup, eSI_reg, DIS_REP_Fixup }
#define Ybr { REP_Fixup, eDI_reg, DIS_REP_Fixup }
#define Yvr { REP_Fixup, eDI_reg, DIS_REP_Fixup }
#define Yzr { REP_Fixup, eDI_reg, DIS_REP_Fixup }
#define indirDXr { REP_Fixup, indir_dx_reg, DIS_REP_Fixup }
#define ALr { REP_Fixup, al_reg, DIS_REP_Fixup }
#define eAXr { REP_Fixup, eAX_reg, DIS_REP_Fixup }
#define Pre { Prefix, 0, DIS_Prefix }

#define cond_jump_flag { NULL, cond_jump_mode }
#define loop_jcxz_flag { NULL, loop_jcxz_mode }

/* bits in sizeflag */
#define SUFFIX_ALWAYS 4
#define AFLAG 2
#define DFLAG 1

#define b_mode 1  /* byte operand */
#define v_mode 2  /* operand size depends on prefixes */
#define w_mode 3  /* word operand */
#define d_mode 4  /* double word operand  */
#define q_mode 5  /* quad word operand */
#define t_mode 6  /* ten-byte operand */
#define x_mode 7  /* 16-byte XMM operand */
#define m_mode 8  /* d_mode in 32bit, q_mode in 64bit mode.  */
#define cond_jump_mode 9
#define loop_jcxz_mode 10
#define dq_mode 11 /* operand size depends on REX prefixes.  */
#define dqw_mode 12 /* registers like dq_mode, memory like w_mode.  */
#define f_mode 13 /* 4- or 6-byte pointer operand */
#define const_1_mode 14
#define stack_v_mode 15 /* v_mode for stack-related opcodes.  */
#define z_mode 16 /* non-quad operand size depends on prefixes */
#define o_mode 17  /* 16-byte operand */
#define dqb_mode 18 /* registers like dq_mode, memory like b_mode.  */
#define dqd_mode 19 /* registers like dq_mode, memory like d_mode.  */

#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105

#define eAX_reg 108
#define eCX_reg 109
#define eDX_reg 110
#define eBX_reg 111
#define eSP_reg 112
#define eBP_reg 113
#define eSI_reg 114
#define eDI_reg 115

#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

#define rAX_reg 132
#define rCX_reg 133
#define rDX_reg 134
#define rBX_reg 135
#define rSP_reg 136
#define rBP_reg 137
#define rSI_reg 138
#define rDI_reg 139

#define z_mode_ax_reg 149
#define indir_dx_reg 150

#define FLOATCODE 1
#define USE_GROUPS 2
#define USE_PREFIX_USER_TABLE 3
#define X86_64_SPECIAL 4
#define IS_3BYTE_OPCODE 5

#define FLOAT	  NULL, { { NULL, FLOATCODE } }

#define GRP1a	  NULL, { { NULL, USE_GROUPS }, { NULL,  0 } }
#define GRP1b	  NULL, { { NULL, USE_GROUPS }, { NULL,  1 } }
#define GRP1S	  NULL, { { NULL, USE_GROUPS }, { NULL,  2 } }
#define GRP1Ss	  NULL, { { NULL, USE_GROUPS }, { NULL,  3 } }
#define GRP2b	  NULL, { { NULL, USE_GROUPS }, { NULL,  4 } }
#define GRP2S	  NULL, { { NULL, USE_GROUPS }, { NULL,  5 } }
#define GRP2b_one NULL, { { NULL, USE_GROUPS }, { NULL,  6 } }
#define GRP2S_one NULL, { { NULL, USE_GROUPS }, { NULL,  7 } }
#define GRP2b_cl  NULL, { { NULL, USE_GROUPS }, { NULL,  8 } }
#define GRP2S_cl  NULL, { { NULL, USE_GROUPS }, { NULL,  9 } }
#define GRP3b	  NULL, { { NULL, USE_GROUPS }, { NULL, 10 } }
#define GRP3S	  NULL, { { NULL, USE_GROUPS }, { NULL, 11 } }
#define GRP4	  NULL, { { NULL, USE_GROUPS }, { NULL, 12 } }
#define GRP5	  NULL, { { NULL, USE_GROUPS }, { NULL, 13 } }
#define GRP6	  NULL, { { NULL, USE_GROUPS }, { NULL, 14 } }
#define GRP7	  NULL, { { NULL, USE_GROUPS }, { NULL, 15 } }
#define GRP8	  NULL, { { NULL, USE_GROUPS }, { NULL, 16 } }
#define GRP9	  NULL, { { NULL, USE_GROUPS }, { NULL, 17 } }
#define GRP11_C6  NULL, { { NULL, USE_GROUPS }, { NULL, 18 } }
#define GRP11_C7  NULL, { { NULL, USE_GROUPS }, { NULL, 19 } }
#define GRP12	  NULL, { { NULL, USE_GROUPS }, { NULL, 20 } }
#define GRP13	  NULL, { { NULL, USE_GROUPS }, { NULL, 21 } }
#define GRP14	  NULL, { { NULL, USE_GROUPS }, { NULL, 22 } }
#define GRP15	  NULL, { { NULL, USE_GROUPS }, { NULL, 23 } }
#define GRP16	  NULL, { { NULL, USE_GROUPS }, { NULL, 24 } }
#define GRPAMD	  NULL, { { NULL, USE_GROUPS }, { NULL, 25 } }
#define GRPPADLCK1 NULL, { { NULL, USE_GROUPS }, { NULL, 26 } }
#define GRPPADLCK2 NULL, { { NULL, USE_GROUPS }, { NULL, 27 } }

#define PREGRP0   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  0 } }
#define PREGRP1   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  1 } }
#define PREGRP2   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  2 } }
#define PREGRP3   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  3 } }
#define PREGRP4   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  4 } }
#define PREGRP5   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  5 } }
#define PREGRP6   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  6 } }
#define PREGRP7   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  7 } }
#define PREGRP8   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  8 } }
#define PREGRP9   NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL,  9 } }
#define PREGRP10  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 10 } }
#define PREGRP11  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 11 } }
#define PREGRP12  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 12 } }
#define PREGRP13  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 13 } }
#define PREGRP14  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 14 } }
#define PREGRP15  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 15 } }
#define PREGRP16  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 16 } }
#define PREGRP17  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 17 } }
#define PREGRP18  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 18 } }
#define PREGRP19  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 19 } }
#define PREGRP20  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 20 } }
#define PREGRP21  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 21 } }
#define PREGRP22  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 22 } }
#define PREGRP23  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 23 } }
#define PREGRP24  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 24 } }
#define PREGRP25  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 25 } }
#define PREGRP26  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 26 } }
#define PREGRP27  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 27 } }
#define PREGRP28  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 28 } }
#define PREGRP29  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 29 } }
#define PREGRP30  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 30 } }
#define PREGRP31  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 31 } }
#define PREGRP32  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 32 } }
#define PREGRP33  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 33 } }
#define PREGRP34  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 34 } }
#define PREGRP35  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 35 } }
#define PREGRP36  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 36 } }
#define PREGRP37  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 37 } }
#define PREGRP38  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 38 } }
#define PREGRP39  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 39 } }
#define PREGRP40  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 40 } }
#define PREGRP41  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 41 } }
#define PREGRP42  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 42 } }
#define PREGRP43  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 43 } }
#define PREGRP44  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 44 } }
#define PREGRP45  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 45 } }
#define PREGRP46  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 46 } }
#define PREGRP47  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 47 } }
#define PREGRP48  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 48 } }
#define PREGRP49  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 49 } }
#define PREGRP50  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 50 } }
#define PREGRP51  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 51 } }
#define PREGRP52  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 52 } }
#define PREGRP53  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 53 } }
#define PREGRP54  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 54 } }
#define PREGRP55  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 55 } }
#define PREGRP56  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 56 } }
#define PREGRP57  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 57 } }
#define PREGRP58  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 58 } }
#define PREGRP59  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 59 } }
#define PREGRP60  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 60 } }
#define PREGRP61  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 61 } }
#define PREGRP62  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 62 } }
#define PREGRP63  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 63 } }
#define PREGRP64  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 64 } }
#define PREGRP65  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 65 } }
#define PREGRP66  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 66 } }
#define PREGRP67  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 67 } }
#define PREGRP68  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 68 } }
#define PREGRP69  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 69 } }
#define PREGRP70  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 70 } }
#define PREGRP71  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 71 } }
#define PREGRP72  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 72 } }
#define PREGRP73  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 73 } }
#define PREGRP74  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 74 } }
#define PREGRP75  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 75 } }
#define PREGRP76  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 76 } }
#define PREGRP77  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 77 } }
#define PREGRP78  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 78 } }
#define PREGRP79  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 79 } }
#define PREGRP80  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 80 } }
#define PREGRP81  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 81 } }
#define PREGRP82  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 82 } }
#define PREGRP83  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 83 } }
#define PREGRP84  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 84 } }
#define PREGRP85  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 85 } }
#define PREGRP86  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 86 } }
#define PREGRP87  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 87 } }
#define PREGRP88  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 88 } }
#define PREGRP89  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 89 } }
#define PREGRP90  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 90 } }
#define PREGRP91  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 91 } }
#define PREGRP92  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 92 } }
#define PREGRP93  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 93 } }
#define PREGRP94  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 94 } }
#define PREGRP95  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 95 } }
#define PREGRP96  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 96 } }
#define PREGRP97  NULL, { { NULL, USE_PREFIX_USER_TABLE }, { NULL, 97 } }


#define X86_64_0  NULL, { { NULL, X86_64_SPECIAL }, { NULL, 0 } }
#define X86_64_1  NULL, { { NULL, X86_64_SPECIAL }, { NULL, 1 } }
#define X86_64_2  NULL, { { NULL, X86_64_SPECIAL }, { NULL, 2 } }
#define X86_64_3  NULL, { { NULL, X86_64_SPECIAL }, { NULL, 3 } }

#define THREE_BYTE_0 NULL, { { NULL, IS_3BYTE_OPCODE }, { NULL, 0 } }
#define THREE_BYTE_1 NULL, { { NULL, IS_3BYTE_OPCODE }, { NULL, 1 } }

typedef void (*op_rtn) (bti386_t *bti386, int bytemode, int sizeflag);
typedef void (*op_disas_rtn)(bti386_t *bti386, operand_t *op, int bytemode, int sizeflag);

#define dp_inval 0
struct dis386 {
	const char *name;
	struct
	{
		op_rtn rtn;
		int bytemode;
		op_disas_rtn disas_rtn;
	} op[MAX_OPERANDS];
	int dpnum;
};

/* Upper case letters in the instruction names here are macros.
	 'A' => print 'b' if no register operands or suffix_always is true
	 'B' => print 'b' if suffix_always is true
	 'C' => print 's' or 'l' ('w' or 'd' in Intel mode) depending on operand
	 .      size prefix
	 'D' => print 'w' if no register operands or 'w', 'l' or 'q', if
	 .      suffix_always is true
	 'E' => print 'e' if 32-bit form of jcxz
	 'F' => print 'w' or 'l' depending on address size prefix (loop insns)
	 'G' => print 'w' or 'l' depending on operand size prefix (i/o insns)
	 'H' => print ",pt" or ",pn" branch hint
	 'I' => honor following macro letter even in Intel mode (implemented only
	 .      for some of the macro letters)
	 'J' => print 'l'
	 'K' => print 'd' or 'q' if rex prefix is present.
	 'L' => print 'l' if suffix_always is true
	 'N' => print 'n' if instruction has no wait "prefix"
	 'O' => print 'd' or 'o' (or 'q' in Intel mode)
	 'P' => print 'w', 'l' or 'q' if instruction has an operand size prefix,
	 .      or suffix_always is true.  print 'q' if rex prefix is present.
	 'Q' => print 'w', 'l' or 'q' if no register operands or suffix_always
	 .      is true
	 'R' => print 'w', 'l' or 'q' ('d' for 'l' and 'e' in Intel mode)
	 'S' => print 'w', 'l' or 'q' if suffix_always is true
	 'T' => print 'q' in 64bit mode and behave as 'P' otherwise
	 'U' => print 'q' in 64bit mode and behave as 'Q' otherwise
	 'V' => print 'q' in 64bit mode and behave as 'S' otherwise
	 'W' => print 'b', 'w' or 'l' ('d' in Intel mode)
	 'X' => print 's', 'd' depending on data16 prefix (for XMM)
	 'Y' => 'q' if instruction has an REX 64bit overwrite prefix
	 'Z' => print 'q' in 64bit mode and behave as 'L' otherwise

	 Many of the above letters print nothing in Intel mode.  See "putop"
	 for the details.

	 Braces '{' and '}', and vertical bars '|', indicate alternative
	 mnemonic strings for AT&T, Intel, X86_64 AT&T, and X86_64 Intel
	 modes.  In cases where there are only two alternatives, the X86_64
	 instruction is reserved, and "(bad)" is printed.
	 */

static struct dis386 dis386[] = {
	/* 00 */
	{ "addB",		{ Eb, Gb } },
	{ "addS",		{ Ev, Gv } },
	{ "addB",		{ Gb, Eb } },
	{ "addS",		{ Gv, Ev } },
	{ "addB",		{ AL, Ib } },
	{ "addS",		{ eAX, Iv } },
	{ "push{T|}",		{ es } },
	{ "pop{T|}",		{ es } },
	/* 08 */
	{ "orB",		{ Eb, Gb } },
	{ "orS",		{ Ev, Gv } },
	{ "orB",		{ Gb, Eb } },
	{ "orS",		{ Gv, Ev } },
	{ "orB",		{ AL, Ib } },
	{ "orS",		{ eAX, Iv } },
	{ "push{T|}",		{ cs } },
	{ "(bad)",		{ XX } },	/* 0x0f extended opcode escape */
	/* 10 */
	{ "adcB",		{ Eb, Gb } },
	{ "adcS",		{ Ev, Gv } },
	{ "adcB",		{ Gb, Eb } },
	{ "adcS",		{ Gv, Ev } },
	{ "adcB",		{ AL, Ib } },
	{ "adcS",		{ eAX, Iv } },
	{ "push{T|}",		{ ss } },
	{ "pop{T|}",		{ ss } },
	/* 18 */
	{ "sbbB",		{ Eb, Gb } },
	{ "sbbS",		{ Ev, Gv } },
	{ "sbbB",		{ Gb, Eb } },
	{ "sbbS",		{ Gv, Ev } },
	{ "sbbB",		{ AL, Ib } },
	{ "sbbS",		{ eAX, Iv } },
	{ "push{T|}",		{ ds } },
	{ "pop{T|}",		{ ds } },
	/* 20 */
	{ "andB",		{ Eb, Gb } },
	{ "andS",		{ Ev, Gv } },
	{ "andB",		{ Gb, Eb } },
	{ "andS",		{ Gv, Ev } },
	{ "andB",		{ AL, Ib } },
	{ "andS",		{ eAX, Iv } },
	{ "(bad)",		{ XX } },	/* SEG ES prefix */
	{ "daa{|}",		{ XX } },
	/* 28 */
	{ "subB",		{ Eb, Gb } },
	{ "subS",		{ Ev, Gv } },
	{ "subB",		{ Gb, Eb } },
	{ "subS",		{ Gv, Ev } },
	{ "subB",		{ AL, Ib } },
	{ "subS",		{ eAX, Iv } },
	{ "(bad)",		{ XX } },	/* SEG CS prefix */
	{ "das{|}",		{ XX } },
	/* 30 */
	{ "xorB",		{ Eb, Gb } },
	{ "xorS",		{ Ev, Gv } },
	{ "xorB",		{ Gb, Eb } },
	{ "xorS",		{ Gv, Ev } },
	{ "xorB",		{ AL, Ib } },
	{ "xorS",		{ eAX, Iv } },
	{ "(bad)",		{ XX } },	/* SEG SS prefix */
	{ "aaa{|}",		{ XX } },
	/* 38 */
	{ "cmpB",		{ Eb, Gb } },
	{ "cmpS",		{ Ev, Gv } },
	{ "cmpB",		{ Gb, Eb } },
	{ "cmpS",		{ Gv, Ev } },
	{ "cmpB",		{ AL, Ib } },
	{ "cmpS",		{ eAX, Iv } },
	{ "(bad)",		{ XX } },	/* SEG DS prefix */
	{ "aas{|}",		{ XX } },
	/* 40 */
	{ "inc{S|}",		{ RMeAX } },
	{ "inc{S|}",		{ RMeCX } },
	{ "inc{S|}",		{ RMeDX } },
	{ "inc{S|}",		{ RMeBX } },
	{ "inc{S|}",		{ RMeSP } },
	{ "inc{S|}",		{ RMeBP } },
	{ "inc{S|}",		{ RMeSI } },
	{ "inc{S|}",		{ RMeDI } },
	/* 48 */
	{ "dec{S|}",		{ RMeAX } },
	{ "dec{S|}",		{ RMeCX } },
	{ "dec{S|}",		{ RMeDX } },
	{ "dec{S|}",		{ RMeBX } },
	{ "dec{S|}",		{ RMeSP } },
	{ "dec{S|}",		{ RMeBP } },
	{ "dec{S|}",		{ RMeSI } },
	{ "dec{S|}",		{ RMeDI } },
	/* 50 */
	{ "pushV",		{ RMrAX } },
	{ "pushV",		{ RMrCX } },
	{ "pushV",		{ RMrDX } },
	{ "pushV",		{ RMrBX } },
	{ "pushV",		{ RMrSP } },
	{ "pushV",		{ RMrBP } },
	{ "pushV",		{ RMrSI } },
	{ "pushV",		{ RMrDI } },
	/* 58 */
	{ "popV",		{ RMrAX } },
	{ "popV",		{ RMrCX } },
	{ "popV",		{ RMrDX } },
	{ "popV",		{ RMrBX } },
	{ "popV",		{ RMrSP } },
	{ "popV",		{ RMrBP } },
	{ "popV",		{ RMrSI } },
	{ "popV",		{ RMrDI } },
	/* 60 */
	{ X86_64_0 },
	{ X86_64_1 },
	{ X86_64_2 },
	{ X86_64_3 },
	{ "(bad)",		{ XX } },	/* seg fs */
	{ "(bad)",		{ XX } },	/* seg gs */
	{ "(bad)",		{ XX } },	/* op size prefix */
	{ "(bad)",		{ XX } },	/* adr size prefix */
	/* 68 */
	{ "pushT",		{ Iq } },
	{ "imulS",		{ Gv, Ev, Iv } },
	{ "pushT",		{ sIb } },
	{ "imulS",		{ Gv, Ev, sIb } },
	{ "ins{b||b|}",	{ Ybr, indirDX, Pre } },
	{ "ins{R||G|}",	{ Yzr, indirDX, Pre } },
	{ "outs{b||b|}",	{ indirDXr, Xb, Pre } },
	{ "outs{R||G|}",	{ indirDXr, Xz, Pre } },
	/* 70 */
	{ "joH",		{ Jb, XX, cond_jump_flag } },
	{ "jnoH",		{ Jb, XX, cond_jump_flag } },
	{ "jbH",		{ Jb, XX, cond_jump_flag } },
	{ "jaeH",		{ Jb, XX, cond_jump_flag } },
	{ "jeH",		{ Jb, XX, cond_jump_flag } },
	{ "jneH",		{ Jb, XX, cond_jump_flag } },
	{ "jbeH",		{ Jb, XX, cond_jump_flag } },
	{ "jaH",		{ Jb, XX, cond_jump_flag } },
	/* 78 */
	{ "jsH",		{ Jb, XX, cond_jump_flag } },
	{ "jnsH",		{ Jb, XX, cond_jump_flag } },
	{ "jpH",		{ Jb, XX, cond_jump_flag } },
	{ "jnpH",		{ Jb, XX, cond_jump_flag } },
	{ "jlH",		{ Jb, XX, cond_jump_flag } },
	{ "jgeH",		{ Jb, XX, cond_jump_flag } },
	{ "jleH",		{ Jb, XX, cond_jump_flag } },
	{ "jgH",		{ Jb, XX, cond_jump_flag } },
	/* 80 */
	{ GRP1b },
	{ GRP1S },
	{ "(bad)",		{ XX } },
	{ GRP1Ss },
	{ "testB",		{ Eb, Gb } },
	{ "testS",		{ Ev, Gv } },
	{ "xchgB",		{ Eb, Gb } },
	{ "xchgS",		{ Ev, Gv } },
	/* 88 */
	{ "movB",		{ Eb, Gb } },
	{ "movS",		{ Ev, Gv } },
	{ "movB",		{ Gb, Eb } },
	{ "movS",		{ Gv, Ev } },
	{ "movD",		{ Sv, Sw } },
	{ "leaS",		{ Gv, M } },
	{ "movD",		{ Sw, Sv } },
	{ GRP1a },
	/* 90 */
	{ PREGRP38 },
	{ "xchgS",		{ RMeCX, eAX } },
	{ "xchgS",		{ RMeDX, eAX } },
	{ "xchgS",		{ RMeBX, eAX } },
	{ "xchgS",		{ RMeSP, eAX } },
	{ "xchgS",		{ RMeBP, eAX } },
	{ "xchgS",		{ RMeSI, eAX } },
	{ "xchgS",		{ RMeDI, eAX } },
	/* 98 */
	{ "cW{t||t|}R",	{ XX } },
	{ "cR{t||t|}O",	{ XX } },
	{ "Jcall{T|}",	{ Ap } },
	{ "(bad)",		{ XX } },	/* fwait */
	{ "pushfT",		{ XX } },
	{ "popfT",		{ XX } },
	{ "sahf{|}",		{ XX } },
	{ "lahf{|}",		{ XX } },
	/* a0 */
	{ "movB",		{ AL, Ob } },
	{ "movS",		{ eAX, Ov } },
	{ "movB",		{ Ob, AL } },
	{ "movS",		{ Ov, eAX } },
	{ "movs{b||b|}",	{ Ybr, Xb, Pre } },
	{ "movs{R||R|}",	{ Yvr, Xv, Pre } },
	{ "cmps{b||b|}",	{ Xb, Yb, Pre } },
	{ "cmps{R||R|}",	{ Xv, Yv, Pre } },
	/* a8 */
	{ "testB",		{ AL, Ib } },
	{ "testS",		{ eAX, Iv } },
	{ "stosB",		{ Ybr, AL, Pre } },
	{ "stosS",		{ Yvr, eAX, Pre } },
	{ "lodsB",		{ ALr, Xb, Pre } },
	{ "lodsS",		{ eAXr, Xv, Pre } },
	{ "scasB",		{ AL, Yb, /*sorav*/Pre } },
	{ "scasS",		{ eAX, Yv, /*sorav*/Pre } },
	/* b0 */
	{ "movB",		{ RMAL, Ib } },
	{ "movB",		{ RMCL, Ib } },
	{ "movB",		{ RMDL, Ib } },
	{ "movB",		{ RMBL, Ib } },
	{ "movB",		{ RMAH, Ib } },
	{ "movB",		{ RMCH, Ib } },
	{ "movB",		{ RMDH, Ib } },
	{ "movB",		{ RMBH, Ib } },
	/* b8 */
	{ "movS",		{ RMeAX, Iv64 } },
	{ "movS",		{ RMeCX, Iv64 } },
	{ "movS",		{ RMeDX, Iv64 } },
	{ "movS",		{ RMeBX, Iv64 } },
	{ "movS",		{ RMeSP, Iv64 } },
	{ "movS",		{ RMeBP, Iv64 } },
	{ "movS",		{ RMeSI, Iv64 } },
	{ "movS",		{ RMeDI, Iv64 } },
	/* c0 */
	{ GRP2b },
	{ GRP2S },
	{ "retT",		{ Iw } },
	{ "retT",		{ XX } },
	{ "les{S|}",		{ Gv, Mp } },
	{ "ldsS",		{ Gv, Mp } },
	{ GRP11_C6 },
	{ GRP11_C7 },
	/* c8 */
	{ "enterT",		{ Iw, Ib } },
	{ "leaveT",		{ XX } },
	{ "lretP",		{ Iw } },
	{ "lretP",		{ XX } },
	{ "int3",		{ XX } },
	{ "int",		{ Ib } },
	{ "into{|}",		{ XX } },
	{ "iretP",		{ XX } },
	/* d0 */
	{ GRP2b_one },
	{ GRP2S_one },
	{ GRP2b_cl },
	{ GRP2S_cl },
	{ "aam{|}",		{ sIb } },
	{ "aad{|}",		{ sIb } },
	{ "(bad)",		{ XX } },
	{ "xlat",		{ DSBX } },
	/* d8 */
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	{ FLOAT },
	/* e0 */
	{ "loopneFH",		{ Jb, XX, loop_jcxz_flag } },
	{ "loopeFH",		{ Jb, XX, loop_jcxz_flag } },
	{ "loopFH",		{ Jb, XX, loop_jcxz_flag } },
	{ "jEcxzH",		{ Jb, XX, loop_jcxz_flag } },
	{ "inB",		{ AL, Ib } },
	{ "inG",		{ zAX, Ib } },
	{ "outB",		{ Ib, AL } },
	{ "outG",		{ Ib, zAX } },
	/* e8 */
	{ "callT",		{ Jv } },
	{ "jmpT",		{ Jv } },
	{ "Jjmp{T|}",		{ Ap } },
	{ "jmp",		{ Jb } },
	{ "inB",		{ AL, indirDX } },
	{ "inG",		{ zAX, indirDX } },
	{ "outB",		{ indirDX, AL } },
	{ "outG",		{ indirDX, zAX } },
	/* f0 */
	{ "(bad)",		{ XX } },	/* lock prefix */
	{ "icebp",		{ XX } },
	{ "(bad)",		{ XX } },	/* repne */
	{ "(bad)",		{ XX } },	/* repz */
	{ "hlt",		{ XX } },
	{ "cmc",		{ XX } },
	{ GRP3b },
	{ GRP3S },
	/* f8 */
	{ "clc",		{ XX } },
	{ "stc",		{ XX } },
	{ "cli",		{ XX } },
	{ "sti",		{ XX } },
	{ "cld",		{ XX } },
	{ "std",		{ XX } },
	{ GRP4 },
	{ GRP5 },
};

static struct dis386 dis386_twobyte[] = {
	/* 00 */
	{ GRP6 },
	{ GRP7 },
	{ "larS",		{ Gv, Ew } },
	{ "lslS",		{ Gv, Ew } },
	{ "(bad)",		{ XX } },
	{ "syscall",		{ XX } },
	{ "clts",		{ XX } },
	{ "sysretP",		{ XX } },
	/* 08 */
	{ "invd",		{ XX } },
	{ "wbinvd",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "ud2a",		{ XX } },
	{ "(bad)",		{ XX } },
	{ GRPAMD },
	{ "femms",		{ XX } },
	{ "",			{ MX, EM, OPSUF } }, /* See OP_3DNowSuffix.  */
	/* 10 */
	{ PREGRP8 },
	{ PREGRP9 },
	{ PREGRP30 },
	{ "movlpX",		{ EXq, XM, { SIMD_Fixup, 'h' } } },
	{ "unpcklpX",		{ XM, EXq } },
	{ "unpckhpX",		{ XM, EXq } },
	{ PREGRP31 },
	{ "movhpX",		{ EXq, XM, { SIMD_Fixup, 'l' } } },
	/* 18 */
	{ GRP16 },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "nopQ",		{ Ev } },
	/* 20 */
	{ "movZ",		{ Rm, Cm } },
	{ "movZ",		{ Rm, Dm } },
	{ "movZ",		{ Cm, Rm } },
	{ "movZ",		{ Dm, Rm } },
	{ "movL",		{ Rd, Td } },
	{ "(bad)",		{ XX } },
	{ "movL",		{ Td, Rd } },
	{ "(bad)",		{ XX } },
	/* 28 */
	{ "movapX",		{ XM, EXx } },
	{ "movapX",		{ EXx,  XM } },
	{ PREGRP2 },
	{ PREGRP33 },
	{ PREGRP4 },
	{ PREGRP3 },
	{ PREGRP93 },
	{ PREGRP94 },
	/* 30 */
	{ "wrmsr",		{ XX } },
	{ "rdtsc",		{ XX } },
	{ "rdmsr",		{ XX } },
	{ "rdpmc",		{ XX } },
	{ "sysenter",		{ XX } },
	{ "sysexit",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	/* 38 */
	{ THREE_BYTE_0 },
	{ "(bad)",		{ XX } },
	{ THREE_BYTE_1 },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	/* 40 */
	{ "cmovo",		{ Gv, Ev } },
	{ "cmovno",		{ Gv, Ev } },
	{ "cmovb",		{ Gv, Ev } },
	{ "cmovae",		{ Gv, Ev } },
	{ "cmove",		{ Gv, Ev } },
	{ "cmovne",		{ Gv, Ev } },
	{ "cmovbe",		{ Gv, Ev } },
	{ "cmova",		{ Gv, Ev } },
	/* 48 */
	{ "cmovs",		{ Gv, Ev } },
	{ "cmovns",		{ Gv, Ev } },
	{ "cmovp",		{ Gv, Ev } },
	{ "cmovnp",		{ Gv, Ev } },
	{ "cmovl",		{ Gv, Ev } },
	{ "cmovge",		{ Gv, Ev } },
	{ "cmovle",		{ Gv, Ev } },
	{ "cmovg",		{ Gv, Ev } },
	/* 50 */
	{ "movmskpX",		{ Gdq, XS } },
	{ PREGRP13 },
	{ PREGRP12 },
	{ PREGRP11 },
	{ "andpX",		{ XM, EXx } },
	{ "andnpX",		{ XM, EXx } },
	{ "orpX",		{ XM, EXx } },
	{ "xorpX",		{ XM, EXx } },
	/* 58 */
	{ PREGRP0 },
	{ PREGRP10 },
	{ PREGRP17 },
	{ PREGRP16 },
	{ PREGRP14 },
	{ PREGRP7 },
	{ PREGRP5 },
	{ PREGRP6 },
	/* 60 */
	{ PREGRP95 },
	{ PREGRP96 },
	{ PREGRP97 },
	{ "packsswb",		{ MX, EM } },
	{ "pcmpgtb",		{ MX, EM } },
	{ "pcmpgtw",		{ MX, EM } },
	{ "pcmpgtd",		{ MX, EM } },
	{ "packuswb",		{ MX, EM } },
	/* 68 */
	{ "punpckhbw",	{ MX, EM } },
	{ "punpckhwd",	{ MX, EM } },
	{ "punpckhdq",	{ MX, EM } },
	{ "packssdw",		{ MX, EM } },
	{ PREGRP26 },
	{ PREGRP24 },
	{ "movK",		{ MX, Edq } },
	{ PREGRP19 },
	/* 70 */
	{ PREGRP22 },
	{ GRP12 },
	{ GRP13 },
	{ GRP14 },
	{ "pcmpeqb",		{ MX, EM } },
	{ "pcmpeqw",		{ MX, EM } },
	{ "pcmpeqd",		{ MX, EM } },
	{ "emms",		{ XX } },
	/* 78 */
	{ PREGRP34 },
	{ PREGRP35 },
	{ "(bad)",		{ XX } },
	{ "(bad)",		{ XX } },
	{ PREGRP28 },
	{ PREGRP29 },
	{ PREGRP23 },
	{ PREGRP20 },
	/* 80 */
	{ "joH",		{ Jv, XX, cond_jump_flag } },
	{ "jnoH",		{ Jv, XX, cond_jump_flag } },
	{ "jbH",		{ Jv, XX, cond_jump_flag } },
	{ "jaeH",		{ Jv, XX, cond_jump_flag } },
	{ "jeH",		{ Jv, XX, cond_jump_flag } },
	{ "jneH",		{ Jv, XX, cond_jump_flag } },
	{ "jbeH",		{ Jv, XX, cond_jump_flag } },
	{ "jaH",		{ Jv, XX, cond_jump_flag } },
	/* 88 */
	{ "jsH",		{ Jv, XX, cond_jump_flag } },
	{ "jnsH",		{ Jv, XX, cond_jump_flag } },
	{ "jpH",		{ Jv, XX, cond_jump_flag } },
	{ "jnpH",		{ Jv, XX, cond_jump_flag } },
	{ "jlH",		{ Jv, XX, cond_jump_flag } },
	{ "jgeH",		{ Jv, XX, cond_jump_flag } },
	{ "jleH",		{ Jv, XX, cond_jump_flag } },
	{ "jgH",		{ Jv, XX, cond_jump_flag } },
	/* 90 */
	{ "seto",		{ Eb } },
	{ "setno",		{ Eb } },
	{ "setb",		{ Eb } },
	{ "setae",		{ Eb } },
	{ "sete",		{ Eb } },
	{ "setne",		{ Eb } },
	{ "setbe",		{ Eb } },
	{ "seta",		{ Eb } },
	/* 98 */
	{ "sets",		{ Eb } },
	{ "setns",		{ Eb } },
	{ "setp",		{ Eb } },
	{ "setnp",		{ Eb } },
	{ "setl",		{ Eb } },
	{ "setge",		{ Eb } },
	{ "setle",		{ Eb } },
	{ "setg",		{ Eb } },
	/* a0 */
	{ "pushT",		{ fs } },
	{ "popT",		{ fs } },
	{ "cpuid",		{ XX } },
	{ "btS",		{ Ev, Gv } },
	{ "shldS",		{ Ev, Gv, Ib } },
	{ "shldS",		{ Ev, Gv, CL } },
	{ GRPPADLCK2 },
	{ GRPPADLCK1 },
	/* a8 */
	{ "pushT",		{ gs } },
	{ "popT",		{ gs } },
	{ "rsm",		{ XX } },
	{ "btsS",		{ Ev, Gv } },
	{ "shrdS",		{ Ev, Gv, Ib } },
	{ "shrdS",		{ Ev, Gv, CL } },
	{ GRP15 },
	{ "imulS",		{ Gv, Ev } },
	/* b0 */
	{ "cmpxchgB",		{ Eb, Gb } },
	{ "cmpxchgS",		{ Ev, Gv } },
	{ "lssS",		{ Gv, Mp } },
	{ "btrS",		{ Ev, Gv } },
	{ "lfsS",		{ Gv, Mp } },
	{ "lgsS",		{ Gv, Mp } },
	{ "movz{bR|x|bR|x}",	{ Gv, Eb } },
	{ "movz{wR|x|wR|x}",	{ Gv, Ew } }, /* yes, there really is movzww ! */
	/* b8 */
	{ PREGRP37 },
	{ "ud2b",		{ XX } },
	{ GRP8 },
	{ "btcS",		{ Ev, Gv } },
	{ "bsfS",		{ Gv, Ev } },
	{ PREGRP36 },
	{ "movs{bR|x|bR|x}",	{ Gv, Eb } },
	{ "movs{wR|x|wR|x}",	{ Gv, Ew } }, /* yes, there really is movsww ! */
	/* c0 */
	{ "xaddB",		{ Eb, Gb } },
	{ "xaddS",		{ Ev, Gv } },
	{ PREGRP1 },
	{ "movntiS",		{ Ev, Gv } },
	{ "pinsrw",		{ MX, Edqw, Ib } },
	{ "pextrw",		{ Gdq, MS, Ib } },
	{ "shufpX",		{ XM, EXx, Ib } },
	{ GRP9 },
	/* c8 */
	{ "bswap",		{ RMeAX } },
	{ "bswap",		{ RMeCX } },
	{ "bswap",		{ RMeDX } },
	{ "bswap",		{ RMeBX } },
	{ "bswap",		{ RMeSP } },
	{ "bswap",		{ RMeBP } },
	{ "bswap",		{ RMeSI } },
	{ "bswap",		{ RMeDI } },
	/* d0 */
	{ PREGRP27 },
	{ "psrlw",		{ MX, EM } },
	{ "psrld",		{ MX, EM } },
	{ "psrlq",		{ MX, EM } },
	{ "paddq",		{ MX, EM } },
	{ "pmullw",		{ MX, EM } },
	{ PREGRP21 },
	{ "pmovmskb",		{ Gdq, MS } },
	/* d8 */
	{ "psubusb",		{ MX, EM } },
	{ "psubusw",		{ MX, EM } },
	{ "pminub",		{ MX, EM } },
	{ "pand",		{ MX, EM } },
	{ "paddusb",		{ MX, EM } },
	{ "paddusw",		{ MX, EM } },
	{ "pmaxub",		{ MX, EM } },
	{ "pandn",		{ MX, EM } },
	/* e0 */
	{ "pavgb",		{ MX, EM } },
	{ "psraw",		{ MX, EM } },
	{ "psrad",		{ MX, EM } },
	{ "pavgw",		{ MX, EM } },
	{ "pmulhuw",		{ MX, EM } },
	{ "pmulhw",		{ MX, EM } },
	{ PREGRP15 },
	{ PREGRP25 },
	/* e8 */
	{ "psubsb",		{ MX, EM } },
	{ "psubsw",		{ MX, EM } },
	{ "pminsw",		{ MX, EM } },
	{ "por",		{ MX, EM } },
	{ "paddsb",		{ MX, EM } },
	{ "paddsw",		{ MX, EM } },
	{ "pmaxsw",		{ MX, EM } },
	{ "pxor",		{ MX, EM } },
	/* f0 */
	{ PREGRP32 },
	{ "psllw",		{ MX, EM } },
	{ "pslld",		{ MX, EM } },
	{ "psllq",		{ MX, EM } },
	{ "pmuludq",		{ MX, EM } },
	{ "pmaddwd",		{ MX, EM } },
	{ "psadbw",		{ MX, EM } },
	{ PREGRP18 },
	/* f8 */
	{ "psubb",		{ MX, EM } },
	{ "psubw",		{ MX, EM } },
	{ "psubd",		{ MX, EM } },
	{ "psubq",		{ MX, EM } },
	{ "paddb",		{ MX, EM } },
	{ "paddw",		{ MX, EM } },
	{ "paddd",		{ MX, EM } },
	{ "(bad)",		{ XX } },
};

static const unsigned char onebyte_has_modrm[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 00 */
	/* 10 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 10 */
	/* 20 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 20 */
	/* 30 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 30 */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
	/* 60 */ 0,0,1,1,0,0,0,0,0,1,0,1,0,0,0,0, /* 60 */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
	/* 80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 80 */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a0 */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b0 */
	/* c0 */ 1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0, /* c0 */
	/* d0 */ 1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* d0 */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* e0 */
	/* f0 */ 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1  /* f0 */
		/*       -------------------------------        */
		/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

static const unsigned char twobyte_has_modrm[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1, /* 0f */
	/* 10 */ 1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1, /* 1f */
	/* 20 */ 1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /* 3f */
	/* 40 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 4f */
	/* 50 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 5f */
	/* 60 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 6f */
	/* 70 */ 1,1,1,1,1,1,1,0,1,1,0,0,1,1,1,1, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 9f */
	/* a0 */ 0,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1, /* af */
	/* b0 */ 1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1, /* bf */
	/* c0 */ 1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* df */
	/* e0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ef */
	/* f0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0  /* ff */
		/*       -------------------------------        */
		/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

static const unsigned char twobyte_uses_DATA_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1, /* 6f */
	/* 70 */ 1,0,0,0,0,0,0,0,1,1,0,0,1,1,1,1, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0  /* ff */
		/*       -------------------------------        */
		/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

static const unsigned char twobyte_uses_REPNZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,1,0,0,0,0,0,0,1,1,1,0,1,1,1,1, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 1,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

static const unsigned char twobyte_uses_REPZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1, /* 6f */
	/* 70 */ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0, /* bf */
	/* c0 */ 0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 38 XX uses DATA prefix.  */
static const unsigned char threebyte_0x38_uses_DATA_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0, /* 0f */
	/* 10 */ 1,0,0,0,1,1,0,1,0,0,0,0,1,1,1,0, /* 1f */
	/* 20 */ 1,1,1,1,1,1,0,0,1,1,1,1,0,0,0,0, /* 2f */
	/* 30 */ 1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1, /* 3f */
	/* 40 */ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 38 XX uses REPNZ prefix.  */
static const unsigned char threebyte_0x38_uses_REPNZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 38 XX uses REPZ prefix.  */
static const unsigned char threebyte_0x38_uses_REPZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 3a XX uses DATA prefix.  */
static const unsigned char threebyte_0x3a_uses_DATA_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1, /* 0f */
	/* 10 */ 0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 3a XX uses REPNZ prefix.  */
static const unsigned char threebyte_0x3a_uses_REPNZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

/* This is used to determine if opcode 0f 3a XX uses REPZ prefix.  */
static const unsigned char threebyte_0x3a_uses_REPZ_prefix[256] = {
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
	/*       -------------------------------        */
	/* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
	/* 10 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
	/* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 2f */
	/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
	/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
	/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 5f */
	/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 6f */
	/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 7f */
	/* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
	/* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
	/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
	/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
	/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
	/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* df */
	/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ef */
	/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ff */
	/*       -------------------------------        */
	/*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
};

//static char obuf[100];
//static char *(obufp);
//static char scratchbuf[100];
//static unsigned char const *bti386->start_codep;
//static unsigned char const *insn_codep;
//static unsigned char const *codep;
/*static struct
	{
	int mod;
	int reg;
	int rm;
	}
	modrm;*/
//static unsigned char need_modrm;

/* If we are accessing mod/rm/reg without need_modrm set, then the
	 values are stale.  Hitting this abort likely indicates that you
	 need to update onebyte_has_modrm or twobyte_has_modrm.  */
#define MODRM_CHECK  if (!(bti386->need_modrm)) ABORT ()

static const char **names64;
static const char **names32;
static const char **names16;
static const char **names8;
static const char **names8rex;
static const char **names_seg;
static const char **index16;

/*static const char *intel_names64[] = {
	"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
	};
	static const char *intel_names32[] = {
	"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
	"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
	};
	static const char *intel_names16[] = {
	"ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
	"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
	};
	static const char *intel_names8[] = {
	"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
	};
	static const char *intel_names8rex[] = {
	"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
	"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
	};
	static const char *intel_names_seg[] = {
	"es", "cs", "ss", "ds", "fs", "gs", "?", "?",
	};
	static const char *intel_index16[] = {
	"bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"
	};*/

static const char *att_names64[] = {
	"%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
	"%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
};
static const char *att_names32[] = {
	"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
	"%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d"
};
static const char *att_names16[] = {
	"%ax", "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di",
	"%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w"
};
static const char *att_names8[] = {
	"%al", "%cl", "%dl", "%bl", "%ah", "%ch", "%dh", "%bh",
};
static const char *att_names8rex[] = {
	"%al", "%cl", "%dl", "%bl", "%spl", "%bpl", "%sil", "%dil",
	"%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b"
};
static const char *att_names_seg[] = {
	"%es", "%cs", "%ss", "%ds", "%fs", "%gs", "%?", "%?",
};
static const char *att_index16[] = {
	"%bx,%si", "%bx,%di", "%bp,%si", "%bp,%di", "%si", "%di", "%bp", "%bx"
};

static struct dis386 grps[][8] = {
	/* GRP1a */
	{
		{ "popU",	{ stackEv } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* GRP1b */
	{
		{ "addA",	{ Eb, Ib } },
		{ "orA",	{ Eb, Ib } },
		{ "adcA",	{ Eb, Ib } },
		{ "sbbA",	{ Eb, Ib } },
		{ "andA",	{ Eb, Ib } },
		{ "subA",	{ Eb, Ib } },
		{ "xorA",	{ Eb, Ib } },
		{ "cmpA",	{ Eb, Ib } },
	},
	/* GRP1S */
	{
		{ "addQ",	{ Ev, Iv } },
		{ "orQ",	{ Ev, Iv } },
		{ "adcQ",	{ Ev, Iv } },
		{ "sbbQ",	{ Ev, Iv } },
		{ "andQ",	{ Ev, Iv } },
		{ "subQ",	{ Ev, Iv } },
		{ "xorQ",	{ Ev, Iv } },
		{ "cmpQ",	{ Ev, Iv } },
	},
	/* GRP1Ss */
	{
		{ "addQ",	{ Ev, sIb } },
		{ "orQ",	{ Ev, sIb } },
		{ "adcQ",	{ Ev, sIb } },
		{ "sbbQ",	{ Ev, sIb } },
		{ "andQ",	{ Ev, sIb } },
		{ "subQ",	{ Ev, sIb } },
		{ "xorQ",	{ Ev, sIb } },
		{ "cmpQ",	{ Ev, sIb } },
	},
	/* GRP2b */
	{
		{ "rolA",	{ Eb, Ib } },
		{ "rorA",	{ Eb, Ib } },
		{ "rclA",	{ Eb, Ib } },
		{ "rcrA",	{ Eb, Ib } },
		{ "shlA",	{ Eb, Ib } },
		{ "shrA",	{ Eb, Ib } },
		{ "(bad)",	{ XX } },
		{ "sarA",	{ Eb, Ib } },
	},
	/* GRP2S */
	{
		{ "rolQ",	{ Ev, Ib } },
		{ "rorQ",	{ Ev, Ib } },
		{ "rclQ",	{ Ev, Ib } },
		{ "rcrQ",	{ Ev, Ib } },
		{ "shlQ",	{ Ev, Ib } },
		{ "shrQ",	{ Ev, Ib } },
		{ "(bad)",	{ XX } },
		{ "sarQ",	{ Ev, Ib } },
	},
	/* GRP2b_one */
	{
		{ "rolA",	{ Eb, I1 } },
		{ "rorA",	{ Eb, I1 } },
		{ "rclA",	{ Eb, I1 } },
		{ "rcrA",	{ Eb, I1 } },
		{ "shlA",	{ Eb, I1 } },
		{ "shrA",	{ Eb, I1 } },
		{ "(bad)",	{ XX } },
		{ "sarA",	{ Eb, I1 } },
	},
	/* GRP2S_one */
	{
		{ "rolQ",	{ Ev, I1 } },
		{ "rorQ",	{ Ev, I1 } },
		{ "rclQ",	{ Ev, I1 } },
		{ "rcrQ",	{ Ev, I1 } },
		{ "shlQ",	{ Ev, I1 } },
		{ "shrQ",	{ Ev, I1 } },
		{ "(bad)",	{ XX } },
		{ "sarQ",	{ Ev, I1 } },
	},
	/* GRP2b_cl */
	{
		{ "rolA",	{ Eb, CL } },
		{ "rorA",	{ Eb, CL } },
		{ "rclA",	{ Eb, CL } },
		{ "rcrA",	{ Eb, CL } },
		{ "shlA",	{ Eb, CL } },
		{ "shrA",	{ Eb, CL } },
		{ "(bad)",	{ XX } },
		{ "sarA",	{ Eb, CL } },
	},
	/* GRP2S_cl */
	{
		{ "rolQ",	{ Ev, CL } },
		{ "rorQ",	{ Ev, CL } },
		{ "rclQ",	{ Ev, CL } },
		{ "rcrQ",	{ Ev, CL } },
		{ "shlQ",	{ Ev, CL } },
		{ "shrQ",	{ Ev, CL } },
		{ "(bad)",	{ XX } },
		{ "sarQ",	{ Ev, CL } },
	},
	/* GRP3b */
	{
		{ "testA",	{ Eb, Ib } },
		{ "(bad)",	{ Eb } },
		{ "notA",	{ Eb } },
		{ "negA",	{ Eb } },
		{ "mulA",	{ Eb } },	/* Don't print the implicit %al register,  */
		{ "imulA",	{ Eb } },	/* to distinguish these opcodes from other */
		{ "divA",	{ Eb } },	/* mul/imul opcodes.  Do the same for div  */
		{ "idivA",	{ Eb } },	/* and idiv for consistency.		   */
	},
	/* GRP3S */
	{
		{ "testQ",	{ Ev, Iv } },
		{ "(bad)",	{ XX } },
		{ "notQ",	{ Ev } },
		{ "negQ",	{ Ev } },
		{ "mulQ",	{ Ev } },	/* Don't print the implicit register.  */
		{ "imulQ",	{ Ev } },
		{ "divQ",	{ Ev } },
		{ "idivQ",	{ Ev } },
	},
	/* GRP4 */
	{
		{ "incA",	{ Eb } },
		{ "decA",	{ Eb } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* GRP5 */
	{
		{ "incQ",	{ Ev } },
		{ "decQ",	{ Ev } },
		{ "callT",	{ indirEv } },
		{ "JcallT",	{ indirEp } },
		{ "jmpT",	{ indirEv } },
		{ "JjmpT",	{ indirEp } },
		{ "pushU",	{ stackEv } },
		{ "(bad)",	{ XX } },
	},
	/* GRP6 */
	{
		{ "sldtD",	{ Sv } },
		{ "strD",	{ Sv } },
		{ "lldt",	{ Ew } },
		{ "ltr",	{ Ew } },
		{ "verr",	{ Ew } },
		{ "verw",	{ Ew } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* GRP7 */
	{
		{ "sgdt{Q|IQ||}", { { VMX_Fixup, 0, DIS_VMX_Fixup } } },
		{ "sidt{Q|IQ||}", { { PNI_Fixup, 0 } } },
		{ "lgdt{Q|Q||}",	 { M } },
		{ "lidt{Q|Q||}",	 { { SVME_Fixup, 0, DIS_SVME_Fixup } } },
		{ "smswD",	{ Sv } },
		{ "(bad)",	{ XX } },
		{ "lmsw",	{ Ew } },
		{ "invlpg",	{ { INVLPG_Fixup, w_mode, DIS_INVLPG_Fixup } } },
	},
	/* GRP8 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "btQ",	{ Ev, Ib } },
		{ "btsQ",	{ Ev, Ib } },
		{ "btrQ",	{ Ev, Ib } },
		{ "btcQ",	{ Ev, Ib } },
	},
	/* GRP9 */
	{
		{ "(bad)",	{ XX } },
		{ "cmpxchg8b", { { CMPXCHG8B_Fixup, q_mode, DIS_CMPXCHG8B_Fixup } } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "",	{ VM } },		/* See OP_VMX.  */
		{ "vmptrst", { Mq } },
	},
	/* GRP11_C6 */
	{
		{ "movA",	{ Eb, Ib } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* GRP11_C7 */
	{
		{ "movQ",	{ Ev, Iv } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",  { XX } },
	},
	/* GRP12 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "psrlw",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
		{ "psraw",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
		{ "psllw",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
	},
	/* GRP13 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "psrld",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
		{ "psrad",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
		{ "pslld",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
	},
	/* GRP14 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "psrlq",	{ MS, Ib } },
		{ "psrldq",	{ MS, Ib } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "psllq",	{ MS, Ib } },
		{ "pslldq",	{ MS, Ib } },
	},
	/* GRP15 */
	{
		{ "fxsave",		{ Ev } },
		{ "fxrstor",	{ Ev } },
		{ "ldmxcsr",	{ Ev } },
		{ "stmxcsr",	{ Ev } },
		{ "xsave",		{ { OP_0fae, 0, DIS_0fae } } },
		{ "lfence",		{ { OP_0fae, 0, DIS_0fae } } },
		{ "mfence",		{ { OP_0fae, 0, DIS_0fae } } },
		{ "clflush",	{ { OP_0fae, 0, DIS_0fae } } },
	},
	/* GRP16 */
	{
		{ "prefetchnta",	{ Ev } },
		{ "prefetcht0",	{ Ev } },
		{ "prefetcht1",	{ Ev } },
		{ "prefetcht2",	{ Ev } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
	},
	/* GRPAMD */
	{
		{ "prefetch",	{ Eb } },
		{ "prefetchw",	{ Eb } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
		{ "(bad)",		{ XX } },
	},
	/* GRPPADLCK1 */
	{
		{ "xstore-rng",	{ { OP_0f07, 0 } } },
		{ "xcrypt-ecb",	{ { OP_0f07, 0 } } },
		{ "xcrypt-cbc",	{ { OP_0f07, 0 } } },
		{ "xcrypt-ctr",	{ { OP_0f07, 0 } } },
		{ "xcrypt-cfb",	{ { OP_0f07, 0 } } },
		{ "xcrypt-ofb",	{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
	},
	/* GRPPADLCK2 */
	{
		{ "montmul",	{ { OP_0f07, 0 } } },
		{ "xsha1",		{ { OP_0f07, 0 } } },
		{ "xsha256",	{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
		{ "(bad)",		{ { OP_0f07, 0 } } },
	}
};

static struct dis386 prefix_user_table[][4] = {
	/* PREGRP0 */
	{
		{ "addps", { XM, EXx } },
		{ "addss", { XM, EXd } },
		{ "addpd", { XM, EXx } },
		{ "addsd", { XM, EXq } },
	},
	/* PREGRP1 */
	{
		{ "", { XM, EXx, OPSIMD } },	/* See OP_SIMD_SUFFIX.  */
		{ "", { XM, EXd, OPSIMD } },
		{ "", { XM, EXx, OPSIMD } },
		{ "", { XM, EXq, OPSIMD } },
	},
	/* PREGRP2 */
	{
		{ "cvtpi2ps", { XM, EMCq } },
		{ "cvtsi2ssY", { XM, Ev } },
		{ "cvtpi2pd", { XM, EMCq } },
		{ "cvtsi2sdY", { XM, Ev } },
	},
	/* PREGRP3 */
	{
		{ "cvtps2pi", { MXC, EXq } },
		{ "cvtss2siY", { Gv, EXd } },
		{ "cvtpd2pi", { MXC, EXx } },
		{ "cvtsd2siY", { Gv, EXq } },
	},
	/* PREGRP4 */
	{
		{ "cvttps2pi", { MXC, EXq } },
		{ "cvttss2siY", { Gv, EXd } },
		{ "cvttpd2pi", { MXC, EXx } },
		{ "cvttsd2siY", { Gv, EXq } },
	},
	/* PREGRP5 */
	{
		{ "divps",	{ XM, EXx } },
		{ "divss",	{ XM, EXd } },
		{ "divpd",	{ XM, EXx } },
		{ "divsd",	{ XM, EXq } },
	},
	/* PREGRP6 */
	{
		{ "maxps",	{ XM, EXx } },
		{ "maxss",	{ XM, EXd } },
		{ "maxpd",	{ XM, EXx } },
		{ "maxsd",	{ XM, EXq } },
	},
	/* PREGRP7 */
	{
		{ "minps",	{ XM, EXx } },
		{ "minss",	{ XM, EXd } },
		{ "minpd",	{ XM, EXx } },
		{ "minsd",	{ XM, EXq } },
	},
	/* PREGRP8 */
	{
		{ "movups",	{ XM, EXx } },
		{ "movss",	{ XM, EXd } },
		{ "movupd",	{ XM, EXx } },
		{ "movsd",	{ XM, EXq } },
	},
	/* PREGRP9 */
	{
		{ "movups",	{ EXx,  XM } },
		{ "movss",	{ EXd,  XM } },
		{ "movupd",	{ EXx,  XM } },
		{ "movsd",	{ EXq,  XM } },
	},
	/* PREGRP10 */
	{
		{ "mulps",	{ XM, EXx } },
		{ "mulss",	{ XM, EXd } },
		{ "mulpd",	{ XM, EXx } },
		{ "mulsd",	{ XM, EXq } },
	},
	/* PREGRP11 */
	{
		{ "rcpps",	{ XM, EXx } },
		{ "rcpss",	{ XM, EXd } },
		{ "(bad)",	{ XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP12 */
	{
		{ "rsqrtps",{ XM, EXx } },
		{ "rsqrtss",{ XM, EXd } },
		{ "(bad)",	{ XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP13 */
	{
		{ "sqrtps", { XM, EXx } },
		{ "sqrtss", { XM, EXd } },
		{ "sqrtpd", { XM, EXx } },
		{ "sqrtsd",	{ XM, EXq } },
	},
	/* PREGRP14 */
	{
		{ "subps",	{ XM, EXx } },
		{ "subss",	{ XM, EXd } },
		{ "subpd",	{ XM, EXx } },
		{ "subsd",	{ XM, EXq } },
	},
	/* PREGRP15 */
	{
		{ "(bad)",	{ XM, EXx } },
		{ "cvtdq2pd", { XM, EXq } },
		{ "cvttpd2dq", { XM, EXx } },
		{ "cvtpd2dq", { XM, EXx } },
	},
	/* PREGRP16 */
	{
		{ "cvtdq2ps", { XM, EXx } },
		{ "cvttps2dq", { XM, EXx } },
		{ "cvtps2dq", { XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP17 */
	{
		{ "cvtps2pd", { XM, EXq } },
		{ "cvtss2sd", { XM, EXd } },
		{ "cvtpd2ps", { XM, EXx } },
		{ "cvtsd2ss", { XM, EXq } },
	},
	/* PREGRP18 */
	{
		{ "maskmovq", { MX, MS } },
		{ "(bad)",	{ XM, EXx } },
		{ "maskmovdqu", { XM, XS } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP19 */
	{
		{ "movq",	{ MX, EM } },
		{ "movdqu",	{ XM, EXx } },
		{ "movdqa",	{ XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP20 */
	{
		{ "movq",	{ EM, MX } },
		{ "movdqu",	{ EXx,  XM } },
		{ "movdqa",	{ EXx,  XM } },
		{ "(bad)",	{ EXx,  XM } },
	},
	/* PREGRP21 */
	{
		{ "(bad)",	{ EXx,  XM } },
		{ "movq2dq",{ XM, MS } },
		{ "movq",	{ EXq, XM } },
		{ "movdq2q",{ MX, XS } },
	},
	/* PREGRP22 */
	{
		{ "pshufw",	{ MX, EM, Ib } },
		{ "pshufhw",{ XM, EXx, Ib } },
		{ "pshufd",	{ XM, EXx, Ib } },
		{ "pshuflw",{ XM, EXx, Ib } },
	},
	/* PREGRP23 */
	{
		{ "movK",	{ Edq, MX } },
		{ "movq",	{ XM, EXq } },
		{ "movK",	{ Edq, XM } },
		{ "(bad)",	{ Ed, XM } },
	},
	/* PREGRP24 */
	{
		{ "(bad)",	{ MX, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "punpckhqdq", { XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP25 */
	{
		{ "movntq",	{ EM, MX } },
		{ "(bad)",	{ EM, XM } },
		{ "movntdq",{ EM, XM } },
		{ "(bad)",	{ EM, XM } },
	},
	/* PREGRP26 */
	{
		{ "(bad)",	{ MX, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "punpcklqdq", { XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
	},
	/* PREGRP27 */
	{
		{ "(bad)",	{ MX, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "addsubpd", { XM, EXx } },
		{ "addsubps", { XM, EXx } },
	},
	/* PREGRP28 */
	{
		{ "(bad)",	{ MX, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "haddpd",	{ XM, EXx } },
		{ "haddps",	{ XM, EXx } },
	},
	/* PREGRP29 */
	{
		{ "(bad)",	{ MX, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "hsubpd",	{ XM, EXx } },
		{ "hsubps",	{ XM, EXx } },
	},
	/* PREGRP30 */
	{
		{ "movlpX",	{ XM, EXq, { SIMD_Fixup, 'h' } } }, /* really only 2 operands */
		{ "movsldup", { XM, EXx } },
		{ "movlpd",	{ XM, EXq } },
		{ "movddup", { XM, EXq } },
	},
	/* PREGRP31 */
	{
		{ "movhpX",	{ XM, EXq, { SIMD_Fixup, 'l' } } },
		{ "movshdup", { XM, EXx } },
		{ "movhpd",	{ XM, EXq } },
		{ "(bad)",	{ XM, EXq } },
	},
	/* PREGRP32 */
	{
		{ "(bad)",	{ XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "(bad)",	{ XM, EXx } },
		{ "lddqu",	{ XM, M } },
	},
	/* PREGRP33 */
	{
		{"movntps", { Ev, XM } },
		{"movntss", { Ed, XM } },
		{"movntpd", { Ev, XM } },
		{"movntsd", { Eq, XM } },
	},

	/* PREGRP34 */
	{
		{"vmread",	{ Em, Gm } },
		{"(bad)",	{ XX } },
		{"extrq",	{ XS, Ib, Ib } },
		{"insertq",	{ XM, XS, Ib, Ib } },
	},

	/* PREGRP35 */
	{
		{"vmwrite",	{ Gm, Em } },
		{"(bad)",	{ XX } },
		{"extrq",	{ XM, XS } },
		{"insertq",	{ XM, XS } },
	},

	/* PREGRP36 */
	{
		{ "bsrS",	{ Gv, Ev } },
		{ "lzcntS",	{ Gv, Ev } },
		{ "bsrS",	{ Gv, Ev } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP37 */
	{
		{ "(bad)", { XX } },
		{ "popcntS", { Gv, Ev } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
	},

	/* PREGRP38 */
	{
		{ "xchgS", { { NOP_Fixup1, eAX_reg, DIS_NOP_Fixup1 }, { NOP_Fixup2, eAX_reg, DIS_NOP_Fixup2 } } },
		{ "pause", { XX } },
		{ "xchgS", { { NOP_Fixup1, eAX_reg, DIS_NOP_Fixup1 }, { NOP_Fixup2, eAX_reg, DIS_NOP_Fixup2 } } },
		{ "(bad)", { XX } },
	},

	/* PREGRP39 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pblendvb", {XM, EXx, XMM0 } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP40 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "blendvps", {XM, EXx, XMM0 } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP41 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "blendvpd", { XM, EXx, XMM0 } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP42 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "ptest",  { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP43 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxbw", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP44 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxbd", { XM, EXd } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP45 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxbq", { XM, EXw } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP46 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxwd", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP47 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxwq", { XM, EXd } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP48 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovsxdq", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP49 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmuldq", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP50 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpeqq", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP51 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "movntdqa", { XM, EM } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP52 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "packusdw", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP53 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxbw", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP54 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxbd", { XM, EXd } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP55 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxbq", { XM, EXw } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP56 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxwd", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP57 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxwq", { XM, EXd } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP58 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmovzxdq", { XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP59 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pminsb",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP60 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pminsd",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP61 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pminuw",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP62 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pminud",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP63 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmaxsb",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP64 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmaxsd",	{ XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP65 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmaxuw", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP66 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmaxud", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP67 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pmulld", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP68 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "phminposuw", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP69 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "roundps", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP70 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "roundpd", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP71 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "roundss", { XM, EXd, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP72 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "roundsd", { XM, EXq, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP73 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "blendps", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP74 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "blendpd", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP75 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pblendw", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP76 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pextrb",	{ Edqb, XM, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP77 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pextrw",	{ Edqw, XM, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP78 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pextrK",	{ Edq, XM, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP79 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "extractps", { Edqd, XM, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP80 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pinsrb",	{ XM, Edqb, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP81 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "insertps", { XM, EXd, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP82 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pinsrK",	{ XM, Edq, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP83 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "dpps",	{ XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP84 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "dppd",	{ XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP85 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "mpsadbw", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP86 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpgtq", { XM, EXx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP87 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "crc32",	{ Gdq, { CRC32_Fixup, b_mode, DIS_CRC32_Fixup } } },	
	},

	/* PREGRP88 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "crc32",	{ Gdq, { CRC32_Fixup, v_mode, DIS_CRC32_Fixup } } },	
	},

	/* PREGRP89 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpestrm", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP90 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpestri", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP91 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpistrm", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP92 */
	{
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "pcmpistri", { XM, EXx, Ib } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP93 */
	{
		{ "ucomiss",{ XM, EXd } }, 
		{ "(bad)",	{ XX } },
		{ "ucomisd",{ XM, EXq } }, 
		{ "(bad)",	{ XX } },
	},

	/* PREGRP94 */
	{
		{ "comiss",	{ XM, EXd } },
		{ "(bad)",	{ XX } },
		{ "comisd",	{ XM, EXq } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP95 */
	{
		{ "punpcklbw",{ MX, EMd } },
		{ "(bad)",	{ XX } },
		{ "punpcklbw",{ MX, EMx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP96 */
	{
		{ "punpcklwd",{ MX, EMd } },
		{ "(bad)",	{ XX } },
		{ "punpcklwd",{ MX, EMx } },
		{ "(bad)",	{ XX } },
	},

	/* PREGRP97 */
	{
		{ "punpckldq",{ MX, EMd } },
		{ "(bad)",	{ XX } },
		{ "punpckldq",{ MX, EMx } },
		{ "(bad)",	{ XX } },
	},
};

static struct dis386 x86_64_table[][2] = {
	{
		{ "pusha{P|}", { XX } },
		{ "(bad)", { XX } },
	},
	{
		{ "popa{P|}", { XX } },
		{ "(bad)", { XX } },
	},
	{
		{ "bound{S|}", { Gv, Ma } },
		{ "(bad)", { XX } },
	},
	{
		{ "arpl", { Ew, Gw } },
		{ "movs{||lq|xd}", { Gv, Ed } },
	},
};

static struct dis386 three_byte_table[][256] = {
	/* THREE_BYTE_0 */
	{
		/* 00 */
		{ "pshufb", { MX, EM } },
		{ "phaddw", { MX, EM } },
		{ "phaddd",	{ MX, EM } },
		{ "phaddsw", { MX, EM } },
		{ "pmaddubsw", { MX, EM } },
		{ "phsubw", { MX, EM } },
		{ "phsubd", { MX, EM } },
		{ "phsubsw", { MX, EM } },
		/* 08 */
		{ "psignb", { MX, EM } },
		{ "psignw", { MX, EM } },
		{ "psignd", { MX, EM } },
		{ "pmulhrsw", { MX, EM } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 10 */
		{ PREGRP39 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ PREGRP40 },
		{ PREGRP41 },
		{ "(bad)", { XX } },
		{ PREGRP42 },
		/* 18 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "pabsb", { MX, EM } },
		{ "pabsw", { MX, EM } },
		{ "pabsd", { MX, EM } },
		{ "(bad)", { XX } },
		/* 20 */
		{ PREGRP43 },
		{ PREGRP44 },
		{ PREGRP45 },
		{ PREGRP46 },
		{ PREGRP47 },
		{ PREGRP48 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 28 */
		{ PREGRP49 },
		{ PREGRP50 },
		{ PREGRP51 },
		{ PREGRP52 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 30 */
		{ PREGRP53 },
		{ PREGRP54 },
		{ PREGRP55 },
		{ PREGRP56 },
		{ PREGRP57 },
		{ PREGRP58 },
		{ "(bad)", { XX } },
		{ PREGRP86 },
		/* 38 */
		{ PREGRP59 },
		{ PREGRP60 },
		{ PREGRP61 },
		{ PREGRP62 },
		{ PREGRP63 },
		{ PREGRP64 },
		{ PREGRP65 },
		{ PREGRP66 },
		/* 40 */
		{ PREGRP67 },
		{ PREGRP68 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 48 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 50 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 58 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 60 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 68 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 70 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 78 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 80 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 88 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 90 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 98 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* a0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* a8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* b0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* b8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* c0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* c8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* d0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* d8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* e0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* e8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* f0 */
		{ PREGRP87 },
		{ PREGRP88 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* f8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
	},
	/* THREE_BYTE_1 */
	{
		/* 00 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 08 */
		{ PREGRP69 },
		{ PREGRP70 },
		{ PREGRP71 },
		{ PREGRP72 },
		{ PREGRP73 },
		{ PREGRP74 },
		{ PREGRP75 },
		{ "palignr", { MX, EM, Ib } },
		/* 10 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ PREGRP76 },
		{ PREGRP77 },
		{ PREGRP78 },
		{ PREGRP79 },
		/* 18 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 20 */
		{ PREGRP80 },
		{ PREGRP81 },
		{ PREGRP82 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 28 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 30 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 38 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 40 */
		{ PREGRP83 },
		{ PREGRP84 },
		{ PREGRP85 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 48 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 50 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 58 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 60 */
		{ PREGRP89 },
		{ PREGRP90 },
		{ PREGRP91 },
		{ PREGRP92 },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 68 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 70 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 78 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 80 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 88 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 90 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* 98 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* a0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* a8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* b0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* b8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* c0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* c8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* d0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* d8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* e0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* e8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* f0 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		/* f8 */
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
		{ "(bad)", { XX } },
	}
};

#define INTERNAL_DISASSEMBLER_ERROR _("<internal disassembler error>")

	static void
ckprefix (bti386_t *bti386)
{
	int newrex;
	bti386->rex = 0;
	bti386->prefixes = 0;
	bti386->used_prefixes = 0;
	bti386->rex_used = 0;
	while (1) {
		newrex = 0;
		switch (dis_ldub(bti386->codep)) {
			/* REX prefixes family.  */
			case 0x40:
			case 0x41:
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4a:
			case 0x4b:
			case 0x4c:
			case 0x4d:
			case 0x4e:
			case 0x4f:
				if (address_mode == mode_64bit)
					newrex = dis_ldub(bti386->codep);
				else
					return;
				break;
			case 0xf3:
				bti386->prefixes |= PREFIX_REPZ;
				break;
			case 0xf2:
				bti386->prefixes |= PREFIX_REPNZ;
				break;
			case 0xf0:
				bti386->prefixes |= PREFIX_LOCK;
				break;
			case 0x2e:
				bti386->prefixes |= PREFIX_CS;
				break;
			case 0x36:
				bti386->prefixes |= PREFIX_SS;
				break;
			case 0x3e:
				bti386->prefixes |= PREFIX_DS;
				break;
			case 0x26:
				bti386->prefixes |= PREFIX_ES;
				break;
			case 0x64:
				bti386->prefixes |= PREFIX_FS;
				break;
			case 0x65:
				bti386->prefixes |= PREFIX_GS;
				break;
			case 0x66:
				bti386->prefixes |= PREFIX_DATA;
				break;
			case 0x67:
				bti386->prefixes |= PREFIX_ADDR;
				break;
			case FWAIT_OPCODE:
				/* fwait is really an instruction.  If there are prefixes
					 before the fwait, they belong to the fwait, *not* to the
					 following instruction.  */
				if (bti386->prefixes || bti386->rex)
				{
					bti386->prefixes |= PREFIX_FWAIT;
					bti386->codep++;
					return;
				}
				bti386->prefixes = PREFIX_FWAIT;
				break;
			default:
				return;
		}
		/* Rex is ignored when followed by another prefix.  */
		if (bti386->rex)
		{
			bti386->rex_used = bti386->rex;
			return;
		}
		bti386->rex = newrex;
		bti386->codep++;
	}
}

/* Return the name of the prefix byte PREF, or NULL if PREF is not a
	 prefix byte.  */

	static const char *
prefix_name (bti386_t *bti386, int pref, int sizeflag)
{
	static const char *rexes [16] =
	{
		"rex",		/* 0x40 */
		"rex.B",		/* 0x41 */
		"rex.X",		/* 0x42 */
		"rex.XB",		/* 0x43 */
		"rex.R",		/* 0x44 */
		"rex.RB",		/* 0x45 */
		"rex.RX",		/* 0x46 */
		"rex.RXB",	/* 0x47 */
		"rex.W",		/* 0x48 */
		"rex.WB",		/* 0x49 */
		"rex.WX",		/* 0x4a */
		"rex.WXB",	/* 0x4b */
		"rex.WR",		/* 0x4c */
		"rex.WRB",	/* 0x4d */
		"rex.WRX",	/* 0x4e */
		"rex.WRXB",	/* 0x4f */
	};

	switch (pref)
	{
		/* REX prefixes family.  */
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4e:
		case 0x4f:
			return rexes [pref - 0x40];
		case 0xf3:
			return "repz";
		case 0xf2:
			return "repnz";
		case 0xf0:
			return "lock";
		case 0x2e:
			return "cs";
		case 0x36:
			return "ss";
		case 0x3e:
			return "ds";
		case 0x26:
			return "es";
		case 0x64:
			return "fs";
		case 0x65:
			return "gs";
		case 0x66:
			return (sizeflag & DFLAG) ? "data16" : "data32";
		case 0x67:
			if (address_mode == mode_64bit) {
				return (sizeflag & AFLAG) ? "addr32" : "addr64";
			} else {
				return (sizeflag & AFLAG) ? "addr16" : "addr32";
			}
		case FWAIT_OPCODE:
			return "fwait";
		default:
			return NULL;
	}
}

//static char bti386->op_out[MAX_OPERANDS][100];
//static int op_ad, bti386->op_index[MAX_OPERANDS];
//static int two_source_ops;
//static bfd_vma op_address[MAX_OPERANDS];
//static bfd_vma op_riprel[MAX_OPERANDS];
//static bfd_vma bti386->start_pc;

/*
 *   On the 386's of 1988, the maximum length of an instruction is 15 bytes.
 *   (see topic "Redundant prefixes" in the "Differences from 8086"
 *   section of the "Virtual 8086 Mode" chapter.)
 * 'pc' should be the address of this instruction, it will
 *   be used to print the target address if this is a relative jump or call
 * The function returns the length of this instruction in bytes.
 */

//static char intel_syntax;
//static char bti386->open_char;
//static char bti386->close_char;
//static char bti386->separator_char;
//static char bti386->scale_char;

	int
sprint_insn (bti386_t *bti386, bfd_vma pc, char *buf, size_t buf_size, unsigned size, bool guest)
{
	const struct dis386 *dp;
	int i;
	char *op_txt[MAX_OPERANDS];
	int needcomma;
	unsigned char uses_DATA_prefix, uses_LOCK_prefix;
	unsigned char uses_REPNZ_prefix, uses_REPZ_prefix;
	int sizeflag;
	struct dis_private priv;
	unsigned char op;
	char *ptr = buf, *end = buf + buf_size;

	address_mode = mode_32bit;
	bti386->intel_syntax = 0;

	switch (size) {
		case 2:
			priv.orig_sizeflag = 0;
			address_mode = mode_16bit;
			break;
		case 4:
			priv.orig_sizeflag = AFLAG | DFLAG;
			address_mode = mode_32bit;
			break;
		default:
			ERR("Invalid size (%d)\n", size);
			ASSERT(0);
			break;
	}
	priv.orig_sizeflag |= SUFFIX_ALWAYS;

	names64 = att_names64;
	names32 = att_names32;
	names16 = att_names16;
	names8 = att_names8;
	names8rex = att_names8rex;
	names_seg = att_names_seg;
	index16 = att_index16;
	bti386->open_char = '(';
	bti386->close_char =  ')';
	bti386->separator_char = ',';
	bti386->scale_char = ',';

	memcpy(priv.the_buffer, (void *)pc, MAX_MNEM_SIZE);
	priv.max_fetched = priv.the_buffer;
	priv.insn_start = pc;

	(bti386->obuf)[0] = 0;
	for (i = 0; i < MAX_OPERANDS; ++i) {
		bti386->op_out[i][0] = 0;
		bti386->op_index[i] = -1;
	}

	bti386->start_pc = pc;
	bti386->start_codep = priv.the_buffer;
	bti386->codep = priv.the_buffer;

	if (0 /*setjmp (priv.bailout) != 0*/) {
		const char *name;

		/* Getting here means we tried for data but didn't get it.  That
			 means we have an incomplete instruction of some sort.  Just
			 print the first byte as a prefix or a .byte pseudo-op.  */
		if (bti386->codep > priv.the_buffer) {
			name = prefix_name (bti386, priv.the_buffer[0], priv.orig_sizeflag);
			if (name != NULL) {
				ptr += snprintf(ptr, end - ptr, "%s", name);
			} else {
				/* Just print the first byte as a .byte instruction.  */
				ptr += snprintf(ptr, end - ptr, ".byte 0x%x",
						(unsigned int) priv.the_buffer[0]);
			}

			return 1;
		}
		return -1;
	}

	(bti386->obufp) = (bti386->obuf);
	ckprefix (bti386);

	bti386->insn_codep = bti386->codep;
	sizeflag = priv.orig_sizeflag;

	bti386->two_source_ops = (dis_ldub(bti386->codep) == 0x62) || (dis_ldub(bti386->codep) == 0xc8);

	if (((bti386->prefixes & PREFIX_FWAIT)
				&& ((dis_ldub(bti386->codep) < 0xd8) || (dis_ldub(bti386->codep) > 0xdf)))
			|| (bti386->rex && bti386->rex_used)) {
		const char *name;

		/* fwait not followed by floating point instruction, or rex followed
			 by other prefixes.  Print the first prefix.  */
		name = prefix_name (bti386, priv.the_buffer[0], priv.orig_sizeflag);
		if (name == NULL) {
			name = INTERNAL_DISASSEMBLER_ERROR;
		}
		ptr += snprintf (ptr, end - ptr, "%s", name);
		return 1;
	}

	op = 0;
	if (dis_ldub(bti386->codep) == 0x0f) {
		unsigned char threebyte;
		threebyte = dis_ldub(++bti386->codep);
		dp = &dis386_twobyte[threebyte];
		(bti386->need_modrm) = twobyte_has_modrm[dis_ldub(bti386->codep)];
		uses_DATA_prefix = twobyte_uses_DATA_prefix[dis_ldub(bti386->codep)];
		uses_REPNZ_prefix = twobyte_uses_REPNZ_prefix[dis_ldub(bti386->codep)];
		uses_REPZ_prefix = twobyte_uses_REPZ_prefix[dis_ldub(bti386->codep)];
		uses_LOCK_prefix = (dis_ldub(bti386->codep) & ~0x02) == 0x20;
		bti386->codep++;
		if (dp->name == NULL && dp->op[0].bytemode == IS_3BYTE_OPCODE) {
			op = dis_ldub(bti386->codep++);
			switch (threebyte) {
				case 0x38:
					uses_DATA_prefix = threebyte_0x38_uses_DATA_prefix[op];
					uses_REPNZ_prefix = threebyte_0x38_uses_REPNZ_prefix[op];
					uses_REPZ_prefix = threebyte_0x38_uses_REPZ_prefix[op];
					break;
				case 0x3a:
					uses_DATA_prefix = threebyte_0x3a_uses_DATA_prefix[op];
					uses_REPNZ_prefix = threebyte_0x3a_uses_REPNZ_prefix[op];
					uses_REPZ_prefix = threebyte_0x3a_uses_REPZ_prefix[op];
					break;
				default:
					break;
			}
		}
	} else {
		dp = &dis386[dis_ldub(bti386->codep)];
		(bti386->need_modrm) = onebyte_has_modrm[dis_ldub(bti386->codep)];
		uses_DATA_prefix = 0;
		uses_REPNZ_prefix = 0;
		/* pause is 0xf3 0x90.  */
		uses_REPZ_prefix = dis_ldub(bti386->codep) == 0x90;
		uses_LOCK_prefix = 0;
		bti386->codep++;
	}

	if (!uses_REPZ_prefix && (bti386->prefixes & PREFIX_REPZ)) {
		oappend (bti386, "repz ");
		bti386->used_prefixes |= PREFIX_REPZ;
	}
	if (!uses_REPNZ_prefix && (bti386->prefixes & PREFIX_REPNZ)) {
		oappend (bti386, "repnz ");
		bti386->used_prefixes |= PREFIX_REPNZ;
	}

	if (!uses_LOCK_prefix && (bti386->prefixes & PREFIX_LOCK)) {
		oappend (bti386, "lock ");
		bti386->used_prefixes |= PREFIX_LOCK;
	}

	if (bti386->prefixes & PREFIX_ADDR) {
		sizeflag ^= AFLAG;
		if (dp->op[2].bytemode != loop_jcxz_mode || bti386->intel_syntax) {
			if ((sizeflag & AFLAG) || address_mode == mode_64bit) {
				oappend (bti386, "addr32 ");
			} else {
				oappend (bti386, "addr16 ");
			}
			bti386->used_prefixes |= PREFIX_ADDR;
		}
	}

	if (!uses_DATA_prefix && (bti386->prefixes & PREFIX_DATA)) {
		sizeflag ^= DFLAG;
		if (dp->op[2].bytemode == cond_jump_mode
				&& dp->op[0].bytemode == v_mode
				&& !bti386->intel_syntax) {
			if (sizeflag & DFLAG) {
				oappend (bti386, "data32 ");
			} else {
				oappend (bti386, "data16 ");
			}
			bti386->used_prefixes |= PREFIX_DATA;
		}
	}

	if (dp->name == NULL && dp->op[0].bytemode == IS_3BYTE_OPCODE) {
		dp = &three_byte_table[dp->op[1].bytemode][op];
		bti386->modrm.mod = (dis_ldub(bti386->codep) >> 6) & 3;
		bti386->modrm.reg = (dis_ldub(bti386->codep) >> 3) & 7;
		bti386->modrm.rm = dis_ldub(bti386->codep) & 7;
	} else if ((bti386->need_modrm)) {
		bti386->modrm.mod = (dis_ldub(bti386->codep) >> 6) & 3;
		bti386->modrm.reg = (dis_ldub(bti386->codep) >> 3) & 7;
		bti386->modrm.rm = dis_ldub(bti386->codep) & 7;
	}

	if (dp->name == NULL && dp->op[0].bytemode == FLOATCODE) {
		dofloat (bti386, sizeflag);
	} else {
		int index;
		if (dp->name == NULL) {
			switch (dp->op[0].bytemode) {
				case USE_GROUPS:
					dp = &grps[dp->op[1].bytemode][bti386->modrm.reg];
					break;

				case USE_PREFIX_USER_TABLE:
					index = 0;
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
					if (bti386->prefixes & PREFIX_REPZ) {
						index = 1;
					} else {
						/* We should check PREFIX_REPNZ and PREFIX_REPZ
							 before PREFIX_DATA.  */
						bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPNZ);
						if (bti386->prefixes & PREFIX_REPNZ) {
							index = 3;
						} else {
							bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
							if (bti386->prefixes & PREFIX_DATA)
								index = 2;
						}
					}
					dp = &prefix_user_table[dp->op[1].bytemode][index];
					break;

				case X86_64_SPECIAL:
					index = address_mode == mode_64bit ? 1 : 0;
					dp = &x86_64_table[dp->op[1].bytemode][index];
					break;

				default:
					oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
					break;
			}
		}

		if (dp->name != NULL && putop (bti386, dp->name, sizeflag) == 0) {
			for (i = 0; i < MAX_OPERANDS; ++i) {
				(bti386->obufp) = bti386->op_out[i];
				bti386->op_ad = MAX_OPERANDS - 1 - i;
				if (dp->op[i].rtn) {
					(*dp->op[i].rtn) (bti386, dp->op[i].bytemode, sizeflag);
				}
			}
		}
	}

	/* See if any prefixes were not used.  If so, print the first one
		 separately.  If we don't do this, we'll wind up printing an
		 instruction stream which does not precisely correspond to the
		 bytes we are disassembling.  */
	if ((bti386->prefixes & ~bti386->used_prefixes) != 0) {
		const char *name;
		name = prefix_name (bti386, priv.the_buffer[0], priv.orig_sizeflag);
		if (name == NULL) {
			name = INTERNAL_DISASSEMBLER_ERROR;
		}
		ptr += snprintf(ptr, end - ptr, "%s", name);
		return 1;
	}
	if (bti386->rex & ~bti386->rex_used) {
		const char *name;
		name = prefix_name (bti386, bti386->rex | 0x40, priv.orig_sizeflag);
		if (name == NULL) {
			name = INTERNAL_DISASSEMBLER_ERROR;
		}
		ptr += snprintf(ptr, end - ptr, "%s ", name);
	}

	(bti386->obufp) = (bti386->obuf) + strlen ((bti386->obuf));
	for (i = strlen ((bti386->obuf)); i < 6; i++) {
		oappend (bti386, " ");
	}
	oappend (bti386, " ");
	ptr += snprintf(ptr, end - ptr, "%s", (bti386->obuf));

	/* The enter and bound instructions are printed with operands in the same
		 order as the intel book; everything else is printed in reverse order.  */
	if (bti386->intel_syntax || bti386->two_source_ops) {
		bfd_vma riprel;

		for (i = 0; i < MAX_OPERANDS; ++i) {
			op_txt[i] = bti386->op_out[i];
		}

		for (i = 0; i < (MAX_OPERANDS >> 1); ++i) {
			bti386->op_ad = bti386->op_index[i];
			bti386->op_index[i] = bti386->op_index[MAX_OPERANDS - 1 - i];
			bti386->op_index[MAX_OPERANDS - 1 - i] = bti386->op_ad;
			riprel = bti386->op_riprel[i];
			bti386->op_riprel[i] = bti386->op_riprel [MAX_OPERANDS - 1 - i];
			bti386->op_riprel[MAX_OPERANDS - 1 - i] = riprel;
		}
	} else {
		for (i = 0; i < MAX_OPERANDS; ++i) {
			op_txt[MAX_OPERANDS - 1 - i] = bti386->op_out[i];
		}
	}

	needcomma = 0;
	for (i = 0; i < MAX_OPERANDS; ++i) {
		if (*op_txt[i]) {
			if (needcomma) {
				ptr += snprintf(ptr, end - ptr, ",");
			}
			if (bti386->op_index[i] != -1 && !bti386->op_riprel[i]) {
				ptr += snprintf(ptr, end - ptr,"%lx",(bfd_vma)(bti386->op_address[bti386->op_index[i]]));
			} else {
				ptr += snprintf(ptr, end - ptr, "%s", op_txt[i]);
			}
			needcomma = 1;
		}
	}

	for (i = 0; i < MAX_OPERANDS; i++) {
		if (bti386->op_index[i] != -1 && bti386->op_riprel[i]) {
			ptr += snprintf(ptr, end - ptr, "        # ");
			ptr += snprintf (ptr, end - ptr, "%lx",
					(bfd_vma) (bti386->start_pc + bti386->codep - bti386->start_codep + bti386->op_address[bti386->op_index[i]]));
			break;
		}
	}
	ASSERT(ptr < end);
	return bti386->codep - priv.the_buffer;
}
/*
	 size_t
	 sprint_iseq_i386(bti386_t *bti386, void *code, size_t size, uint32_t start_addr,
	 char *buf, size_t buf_size)
	 {
	 char *ptr = buf, *end = ptr + buf_size;
	 unsigned long pc;
	 int count;

 *ptr = '\0';
 for (pc = (unsigned long)code; pc < (unsigned long)code + size; pc += count) {

 ptr += snprintf(ptr, end - ptr, "0x%08lx:  ", pc - start_addr);
//count = __snprint_insn_i386(ptr, end - ptr, (bfd_vma)pc, &disasm_info);
count = sprint_insn(bti386, (bfd_vma)pc, ptr, end - ptr);
ptr += strlen(ptr);
ptr += snprintf(ptr, end - ptr, "\n");
if (count < 0) {
break;
}
}
return ptr - buf;
}
*/
static const char *float_mem[] = {
	/* d8 */
	"fadd{s||s|}",
	"fmul{s||s|}",
	"fcom{s||s|}",
	"fcomp{s||s|}",
	"fsub{s||s|}",
	"fsubr{s||s|}",
	"fdiv{s||s|}",
	"fdivr{s||s|}",
	/* d9 */
	"fld{s||s|}",
	"(bad)",
	"fst{s||s|}",
	"fstp{s||s|}",
	"fldenvIC",
	"fldcw",
	"fNstenvIC",
	"fNstcw",
	/* da */
	"fiadd{l||l|}",
	"fimul{l||l|}",
	"ficom{l||l|}",
	"ficomp{l||l|}",
	"fisub{l||l|}",
	"fisubr{l||l|}",
	"fidiv{l||l|}",
	"fidivr{l||l|}",
	/* db */
	"fild{l||l|}",
	"fisttp{l||l|}",
	"fist{l||l|}",
	"fistp{l||l|}",
	"(bad)",
	"fld{t||t|}",
	"(bad)",
	"fstp{t||t|}",
	/* dc */
	"fadd{l||l|}",
	"fmul{l||l|}",
	"fcom{l||l|}",
	"fcomp{l||l|}",
	"fsub{l||l|}",
	"fsubr{l||l|}",
	"fdiv{l||l|}",
	"fdivr{l||l|}",
	/* dd */
	"fld{l||l|}",
	"fisttp{ll||ll|}",
	"fst{l||l|}",
	"fstp{l||l|}",
	"frstorIC",
	"(bad)",
	"fNsaveIC",
	"fNstsw",
	/* de */
	"fiadd",
	"fimul",
	"ficom",
	"ficomp",
	"fisub",
	"fisubr",
	"fidiv",
	"fidivr",
	/* df */
	"fild",
	"fisttp",
	"fist",
	"fistp",
	"fbld",
	"fild{ll||ll|}",
	"fbstp",
	"fistp{ll||ll|}",
};

static const unsigned char float_mem_mode[] = {
	/* d8 */
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	/* d9 */
	d_mode,
	0,
	d_mode,
	d_mode,
	0,
	w_mode,
	0,
	w_mode,
	/* da */
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	/* db */
	d_mode,
	d_mode,
	d_mode,
	d_mode,
	0,
	t_mode,
	0,
	t_mode,
	/* dc */
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	/* dd */
	q_mode,
	q_mode,
	q_mode,
	q_mode,
	0,
	0,
	0,
	w_mode,
	/* de */
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	/* df */
	w_mode,
	w_mode,
	w_mode,
	w_mode,
	t_mode,
	q_mode,
	t_mode,
	q_mode
};

int float_mem_dpnum[sizeof float_mem_mode/sizeof float_mem_mode[0]];

//#define ST { OP_ST, 0 }
//#define STi { OP_STi, 0 }

#define ST { OP_ST, 0, DIS_ST }
#define STi { OP_STi, 0, DIS_STi }

#define FGRPd9_2 NULL, { { NULL, 0 } }
#define FGRPd9_4 NULL, { { NULL, 1 } }
#define FGRPd9_5 NULL, { { NULL, 2 } }
#define FGRPd9_6 NULL, { { NULL, 3 } }
#define FGRPd9_7 NULL, { { NULL, 4 } }
#define FGRPda_5 NULL, { { NULL, 5 } }
#define FGRPdb_4 NULL, { { NULL, 6 } }
#define FGRPde_3 NULL, { { NULL, 7 } }
#define FGRPdf_4 NULL, { { NULL, 8 } }

static struct dis386 float_reg[][8] = {
	/* d8 */
	{
		{ "fadd",	{ ST, STi } },
		{ "fmul",	{ ST, STi } },
		{ "fcom",	{ STi } },
		{ "fcomp",	{ STi } },
		{ "fsub",	{ ST, STi } },
		{ "fsubr",	{ ST, STi } },
		{ "fdiv",	{ ST, STi } },
		{ "fdivr",	{ ST, STi } },
	},
	/* d9 */
	{
		{ "fld",	{ STi } },
		{ "fxch",	{ STi } },
		{ FGRPd9_2 },
		{ "(bad)",	{ XX } },
		{ FGRPd9_4 },
		{ FGRPd9_5 },
		{ FGRPd9_6 },
		{ FGRPd9_7 },
	},
	/* da */
	{
		{ "fcmovb",	{ ST, STi } },
		{ "fcmove",	{ ST, STi } },
		{ "fcmovbe",{ ST, STi } },
		{ "fcmovu",	{ ST, STi } },
		{ "(bad)",	{ XX } },
		{ FGRPda_5 },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* db */
	{
		{ "fcmovnb",{ ST, STi } },
		{ "fcmovne",{ ST, STi } },
		{ "fcmovnbe",{ ST, STi } },
		{ "fcmovnu",{ ST, STi } },
		{ FGRPdb_4 },
		{ "fucomi",	{ ST, STi } },
		{ "fcomi",	{ ST, STi } },
		{ "(bad)",	{ XX } },
	},
	/* dc */
	{
		{ "fadd",	{ STi, ST } },
		{ "fmul",	{ STi, ST } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
#if SYSV386_COMPAT
		{ "fsub",	{ STi, ST } },
		{ "fsubr",	{ STi, ST } },
		{ "fdiv",	{ STi, ST } },
		{ "fdivr",	{ STi, ST } },
#else
		{ "fsubr",	{ STi, ST } },
		{ "fsub",	{ STi, ST } },
		{ "fdivr",	{ STi, ST } },
		{ "fdiv",	{ STi, ST } },
#endif
	},
	/* dd */
	{
		{ "ffree",	{ STi } },
		{ "(bad)",	{ XX } },
		{ "fst",	{ STi } },
		{ "fstp",	{ STi } },
		{ "fucom",	{ STi } },
		{ "fucomp",	{ STi } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
	},
	/* de */
	{
		{ "faddp",	{ STi, ST } },
		{ "fmulp",	{ STi, ST } },
		{ "(bad)",	{ XX } },
		{ FGRPde_3 },
#if SYSV386_COMPAT
		{ "fsubp",	{ STi, ST } },
		{ "fsubrp",	{ STi, ST } },
		{ "fdivp",	{ STi, ST } },
		{ "fdivrp",	{ STi, ST } },
#else
		{ "fsubrp",	{ STi, ST } },
		{ "fsubp",	{ STi, ST } },
		{ "fdivrp",	{ STi, ST } },
		{ "fdivp",	{ STi, ST } },
#endif
	},
	/* df */
	{
		{ "ffreep",	{ STi } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ "(bad)",	{ XX } },
		{ FGRPdf_4 },
		{ "fucomip", { ST, STi } },
		{ "fcomip", { ST, STi } },
		{ "(bad)",	{ XX } },
	},
};

static char *fgrps[][8] = {
	/* d9_2  0 */
	{
		"fnop","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
	},

	/* d9_4  1 */
	{
		"fchs","fabs","(bad)","(bad)","ftst","fxam","(bad)","(bad)",
	},

	/* d9_5  2 */
	{
		"fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz","(bad)",
	},

	/* d9_6  3 */
	{
		"f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp",
	},

	/* d9_7  4 */
	{
		"fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos",
	},

	/* da_5  5 */
	{
		"(bad)","fucompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
	},

	/* db_4  6 */
	{
		"feni(287 only)","fdisi(287 only)","fNclex","fNinit",
		"fNsetpm(287 only)","(bad)","(bad)","(bad)",
	},

	/* de_3  7 */
	{
		"(bad)","fcompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
	},

	/* df_4  8 */
	{
		"fNstsw","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
	},
};

static int fgrps_dpnum[sizeof fgrps/sizeof fgrps[0]][8];

	static void
dofloat (bti386_t *bti386, int sizeflag)
{
	const struct dis386 *dp;
	unsigned char floatop;

	floatop = dis_ldub(bti386->codep - 1);

	if (bti386->modrm.mod != 3) {
		int fp_indx = (floatop - 0xd8) * 8 + bti386->modrm.reg;

		putop (bti386, float_mem[fp_indx], sizeflag);
		(bti386->obufp) = bti386->op_out[0];
		bti386->op_ad = 2;
		OP_E (bti386, float_mem_mode[fp_indx], sizeflag);
		return;
	}
	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;

	dp = &float_reg[floatop - 0xd8][bti386->modrm.reg];
	if (dp->name == NULL) {
		putop (bti386, fgrps[dp->op[0].bytemode][bti386->modrm.rm], sizeflag);

		/* Instruction fnstsw is only one with strange arg.  */
		if (floatop == 0xdf && dis_ldub(bti386->codep -1) == 0xe0) {
			strlcpy (bti386->op_out[0], names16[0], sizeof bti386->op_out[0]);
		}
	}
	else
	{
		putop (bti386, dp->name, sizeflag);

		(bti386->obufp) = bti386->op_out[0];
		bti386->op_ad = 2;
		if (dp->op[0].rtn)
			(*dp->op[0].rtn) (bti386, dp->op[0].bytemode, sizeflag);

		(bti386->obufp) = bti386->op_out[1];
		bti386->op_ad = 1;
		if (dp->op[1].rtn)
			(*dp->op[1].rtn) (bti386, dp->op[1].bytemode, sizeflag);
	}
}

	static void
disas_dofloat (btmod_vcpu *v, insn_t *insn, int sizeflag)
{
	const struct dis386 *dp;
	unsigned char floatop;
	bti386_t *bti386 = &v->bti386;

	floatop = dis_ldub(bti386->codep -1);

	if (bti386->modrm.mod != 3) {
		int fp_indx = (floatop - 0xd8) * 8 + bti386->modrm.reg;
		int i;

		insn->opc = opctable_find(v->bt, float_mem_dpnum[fp_indx], sizeflag);
		ASSERT(insn->opc != -1);
		(bti386->obufp) = bti386->op_out[0];
		bti386->op_ad = 2;
		DIS_E (bti386, &insn->op[0], float_mem_mode[fp_indx], sizeflag);
		for (i = 1; i < MAX_NUM_OPERANDS; i++) {
			insn->op[i].type = invalid;
		}
		return;
	}
	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;

	dp = &float_reg[floatop - 0xd8][bti386->modrm.reg];
	if (dp->name == NULL) {
		insn->opc = opctable_find(v->bt, fgrps_dpnum[dp->op[0].bytemode][bti386->modrm.rm],
				sizeflag);

		/* Instruction fnstsw is only one with strange arg.  */
		if (floatop == 0xdf && dis_ldub(bti386->codep -1) == 0xe0) {
			insn->op[0].type = op_reg;
			insn->op[0].tag.all = tag_const;
			insn->op[0].val.reg = 0;
			insn->op[0].size = 2;
		}
	} else {
		int i;
		insn->opc = opctable_find(v->bt, dp->dpnum, sizeflag);
		ASSERT(insn->opc != -1);

		for (i = 0; i < 2; i++) {
			bti386->obufp = bti386->op_out[i];
			bti386->op_ad = 2 - i;
			if (dp->op[i].disas_rtn) {
				(*dp->op[i].disas_rtn)(bti386, &insn->op[i], dp->op[i].bytemode, sizeflag);
			} else {
				insn->op[i].type = invalid;
			}
		}
		for (i = 2; i < MAX_NUM_OPERANDS; i++) {
			insn->op[i].type = invalid;
		}
	}
}

	static void
OP_ST (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	oappend (bti386, "%st" + bti386->intel_syntax);
}

	static void
OP_STi (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%st(%d)", bti386->modrm.rm);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

/* Capital letters in template are macros.  */
	static int
putop (bti386_t *bti386, const char *template, int sizeflag)
{
	const char *p;
	int alt = 0;

	for (p = template; *p; p++)
	{
		switch (*p)
		{
			default:
				*(bti386->obufp)++ = *p;
				break;
			case '{':
				alt = 0;
				if (bti386->intel_syntax)
					alt += 1;
				if (address_mode == mode_64bit)
					alt += 2;
				while (alt != 0)
				{
					while (*++p != '|')
					{
						if (*p == '}')
						{
							/* Alternative not valid.  */
							strlcpy ((bti386->obuf), "(bad)", sizeof (bti386->obuf));
							(bti386->obufp) = (bti386->obuf) + 5;
							return 1;
						}
						else if (*p == '\0')
							ABORT ();
					}
					alt--;
				}
				/* Fall through.  */
			case 'I':
				alt = 1;
				continue;
			case '|':
				while (*++p != '}')
				{
					if (*p == '\0')
						ABORT ();
				}
				break;
			case '}':
				break;
			case 'A':
				if (bti386->intel_syntax)
					break;
				if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
					*(bti386->obufp)++ = 'b';
				break;
			case 'B':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
					*(bti386->obufp)++ = 'b';
				break;
			case 'C':
				if (bti386->intel_syntax && !alt)
					break;
				if ((bti386->prefixes & PREFIX_DATA) || (sizeflag & SUFFIX_ALWAYS))
				{
					if (sizeflag & DFLAG)
						*(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
					else
						*(bti386->obufp)++ = bti386->intel_syntax ? 'w' : 's';
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				}
				break;
			case 'D':
				if (bti386->intel_syntax || !(sizeflag & SUFFIX_ALWAYS))
					break;
				USED_REX (REX_W);
				if (bti386->modrm.mod == 3)
				{
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else if (sizeflag & DFLAG)
						*(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
					else
						*(bti386->obufp)++ = 'w';
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				}
				else
					*(bti386->obufp)++ = 'w';
				break;
			case 'E':		/* For jcxz/jecxz */
				if (address_mode == mode_64bit)
				{
					if (sizeflag & AFLAG)
						*(bti386->obufp)++ = 'r';
					else
						*(bti386->obufp)++ = 'e';
				}
				else
					if (sizeflag & AFLAG)
						*(bti386->obufp)++ = 'e';
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_ADDR);
				break;
			case 'F':
				if (bti386->intel_syntax)
					break;
				if ((bti386->prefixes & PREFIX_ADDR) || (sizeflag & SUFFIX_ALWAYS))
				{
					if (sizeflag & AFLAG)
						*(bti386->obufp)++ = address_mode == mode_64bit ? 'q' : 'l';
					else
						*(bti386->obufp)++ = address_mode == mode_64bit ? 'l' : 'w';
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_ADDR);
				}
				break;
			case 'G':
				if (bti386->intel_syntax || ((bti386->obufp)[-1] != 's' && !(sizeflag & SUFFIX_ALWAYS)))
					break;
				if ((bti386->rex & REX_W) || (sizeflag & DFLAG))
					*(bti386->obufp)++ = 'l';
				else
					*(bti386->obufp)++ = 'w';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'H':
				if (bti386->intel_syntax)
					break;
				if ((bti386->prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_CS
						|| (bti386->prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_DS)
				{
					bti386->used_prefixes |= bti386->prefixes & (PREFIX_CS | PREFIX_DS);
					*(bti386->obufp)++ = ',';
					*(bti386->obufp)++ = 'p';
					if (bti386->prefixes & PREFIX_DS)
						*(bti386->obufp)++ = 't';
					else
						*(bti386->obufp)++ = 'n';
				}
				break;
			case 'J':
				if (bti386->intel_syntax)
					break;
				*(bti386->obufp)++ = 'l';
				break;
			case 'K':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'q';
				else
					*(bti386->obufp)++ = 'd';
				break;
			case 'Z':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & SUFFIX_ALWAYS))
				{
					*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'L':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
					*(bti386->obufp)++ = 'l';
				break;
			case 'N':
				if ((bti386->prefixes & PREFIX_FWAIT) == 0)
					*(bti386->obufp)++ = 'n';
				else
					bti386->used_prefixes |= PREFIX_FWAIT;
				break;
			case 'O':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'o';
				else if (bti386->intel_syntax && (sizeflag & DFLAG))
					*(bti386->obufp)++ = 'q';
				else
					*(bti386->obufp)++ = 'd';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'T':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'P':
				if (bti386->intel_syntax)
					break;
				if ((bti386->prefixes & PREFIX_DATA)
						|| (bti386->rex & REX_W)
						|| (sizeflag & SUFFIX_ALWAYS))
				{
					USED_REX (REX_W);
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else
					{
						if (sizeflag & DFLAG)
							*(bti386->obufp)++ = 'l';
						else
							*(bti386->obufp)++ = 'w';
					}
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				}
				break;
			case 'U':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
						*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'Q':
				if (bti386->intel_syntax && !alt)
					break;
				USED_REX (REX_W);
				if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
				{
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else
					{
						if (sizeflag & DFLAG)
							*(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
						else
							*(bti386->obufp)++ = 'w';
					}
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				}
				break;
			case 'R':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'q';
				else if (sizeflag & DFLAG)
				{
					if (bti386->intel_syntax)
						*(bti386->obufp)++ = 'd';
					else
						*(bti386->obufp)++ = 'l';
				}
				else
					*(bti386->obufp)++ = 'w';
				if (bti386->intel_syntax && !p[1]
						&& ((bti386->rex & REX_W) || (sizeflag & DFLAG)))
					*(bti386->obufp)++ = 'e';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'V':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					if (sizeflag & SUFFIX_ALWAYS)
						*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'S':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
				{
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else
					{
						if (sizeflag & DFLAG)
							*(bti386->obufp)++ = 'l';
						else
							*(bti386->obufp)++ = 'w';
						bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
					}
				}
				break;
			case 'X':
				if (bti386->prefixes & PREFIX_DATA)
					*(bti386->obufp)++ = 'd';
				else
					*(bti386->obufp)++ = 's';
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'Y':
				if (bti386->intel_syntax)
					break;
				if (bti386->rex & REX_W)
				{
					USED_REX (REX_W);
					*(bti386->obufp)++ = 'q';
				}
				break;
				/* implicit operand size 'l' for i386 or 'q' for x86-64 */
			case 'W':
				/* operand size flag for cwtl, cbtw */
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
				{
					if (bti386->intel_syntax)
						*(bti386->obufp)++ = 'd';
					else
						*(bti386->obufp)++ = 'l';
				}
				else if (sizeflag & DFLAG)
					*(bti386->obufp)++ = 'w';
				else
					*(bti386->obufp)++ = 'b';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
		}
		alt = 0;
	}
	*(bti386->obufp) = 0;
	return 0;
}

	static int
opc_touches_stack(bti386_t *bti386, char const *opc)
{
	if (   strstart(opc, "call", NULL)
			|| strstart(opc, "ret", NULL)
			|| strstart(opc, "push", NULL)
			|| strstart(opc, "Jcall", NULL)
			|| strstart(opc, "lret", NULL)
			|| strstart(opc, "pop", NULL)) {
		return 1;
	}
	return 0;
}

	static int
opc_touches_cx(bti386_t *bti386, char const *opc)
{
	if (strstart(opc, "loop", NULL)) {
		return 1;
	}
	return 0;
}

	static int
opc_is_size_sensitive(bti386_t *bti386, char const *opc)
{
	return opc_touches_stack(bti386, opc) || opc_touches_cx(bti386, opc);
}

/* Capital letters in template are macros.  */
	static int
disas_putop (bti386_t *bti386, const char *template, int sizeflag)
{
	const char *p;
	int alt = 0;
	//char const *start = (bti386->obufp);

	for (p = template; *p; p++)
	{
		switch (*p)
		{
			default:
				*(bti386->obufp)++ = *p;
				break;
			case '{':
				alt = 0;
				if (bti386->intel_syntax)
					alt += 1;
				if (address_mode == mode_64bit)
					alt += 2;
				while (alt != 0)
				{
					while (*++p != '|')
					{
						if (*p == '}')
						{
							/* Alternative not valid.  */
							strlcpy (bti386->obuf, "(bad)", sizeof (bti386->obuf));
							(bti386->obufp) = (bti386->obuf) + 5;
							return 1;
						}
						else if (*p == '\0')
							ABORT ();
					}
					alt--;
				}
				/* Fall through.  */
			case 'I':
				alt = 1;
				continue;
			case '|':
				while (*++p != '}')
				{
					if (*p == '\0')
						ABORT ();
				}
				break;
			case '}':
				break;
			case 'A':
				if (bti386->intel_syntax)
					break;
				if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
					*(bti386->obufp)++ = 'b';
				break;
			case 'B':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
					*(bti386->obufp)++ = 'b';
				break;
			case 'C':
				/*
					 if (bti386->intel_syntax && !alt)
					 break;
					 if ((bti386->prefixes & PREFIX_DATA) || (sizeflag & SUFFIX_ALWAYS))
					 {
					 if (sizeflag & DFLAG)
				 *(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
				 else
				 *(bti386->obufp)++ = bti386->intel_syntax ? 'w' : 's';
				 bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				 }
				 */
				break;
			case 'D':
				/*
					 if (bti386->intel_syntax || !(sizeflag & SUFFIX_ALWAYS))
					 break;
					 USED_REX (REX_W);
					 if (bti386->modrm.mod == 3)
					 {
					 if (bti386->rex & REX_W)
				 *(bti386->obufp)++ = 'q';
				 else if (sizeflag & DFLAG)
				 *(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
				 else
				 *(bti386->obufp)++ = 'w';
				 bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				 }
				 else
				 *(bti386->obufp)++ = 'w';
				 */
				break;
			case 'E':		/* For jcxz/jecxz */
				/*
					 if (address_mode == mode_64bit)
					 {
					 if (sizeflag & AFLAG)
				 *(bti386->obufp)++ = 'r';
				 else
				 *(bti386->obufp)++ = 'e';
				 }
				 else
				 if (sizeflag & AFLAG)
				 *(bti386->obufp)++ = 'e';
				 bti386->used_prefixes |= (bti386->prefixes & PREFIX_ADDR);
				 */
				break;
			case 'F':
				if (bti386->intel_syntax)
					break;
				if (/*(bti386->prefixes & PREFIX_ADDR) || */(sizeflag & SUFFIX_ALWAYS)
						|| opc_is_size_sensitive(bti386, template)) {
					if (sizeflag & AFLAG)
						*(bti386->obufp)++ = address_mode == mode_64bit ? 'q' : 'l';
					else
						*(bti386->obufp)++ = address_mode == mode_64bit ? 'l' : 'w';
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_ADDR);
				}
				break;
			case 'G':
				/*
					 if (bti386->intel_syntax || ((bti386->obufp)[-1] != 's' && !(sizeflag & SUFFIX_ALWAYS)))
					 break;
					 if ((bti386->rex & REX_W) || (sizeflag & DFLAG))
				 *(bti386->obufp)++ = 'l';
				 else
				 *(bti386->obufp)++ = 'w';
				 if (!(bti386->rex & REX_W))
				 bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				 */
				break;
			case 'H':
				/*
					 if (bti386->intel_syntax)
					 break;
					 if ((bti386->prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_CS
					 || (bti386->prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_DS)
					 {
					 bti386->used_prefixes |= bti386->prefixes & (PREFIX_CS | PREFIX_DS);
				 *(bti386->obufp)++ = ',';
				 *(bti386->obufp)++ = 'p';
				 if (bti386->prefixes & PREFIX_DS)
				 *(bti386->obufp)++ = 't';
				 else
				 *(bti386->obufp)++ = 'n';
				 }
				 */
				break;
			case 'J':
				if (bti386->intel_syntax)
					break;
				*(bti386->obufp)++ = 'l';
				break;
			case 'K':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'q';
				else
					*(bti386->obufp)++ = 'd';
				break;
			case 'Z':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & SUFFIX_ALWAYS))
				{
					*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'L':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
					*(bti386->obufp)++ = 'l';
				break;
			case 'N':
				if ((bti386->prefixes & PREFIX_FWAIT) == 0)
					*(bti386->obufp)++ = 'n';
				else
					bti386->used_prefixes |= PREFIX_FWAIT;
				break;
			case 'O':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'o';
				else if (bti386->intel_syntax && (sizeflag & DFLAG))
					*(bti386->obufp)++ = 'q';
				else
					*(bti386->obufp)++ = 'd';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'T':
				if (bti386->intel_syntax)
					break;
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					*(bti386->obufp)++ = 'q';
					break;
				}
				/* Fall through.  */
			case 'P':
				/*if (bti386->intel_syntax)
					break;
					if (//(bti386->prefixes & PREFIX_DATA)
					|| (bti386->rex & REX_W)
					|| (sizeflag & SUFFIX_ALWAYS)
					|| opc_is_size_sensitive(bti386, template))
					{
					USED_REX (REX_W);
					if (bti386->rex & REX_W)
				 *(bti386->obufp)++ = 'q';
				 else
				 {
				 if (sizeflag & DFLAG)
				 *(bti386->obufp)++ = 'l';
				 else
				 *(bti386->obufp)++ = 'w';
				 }
				 bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				 }*/
				break;
			case 'U':
				/*
					 if (bti386->intel_syntax)
					 break;
					 if (address_mode == mode_64bit && (sizeflag & DFLAG))
					 {
					 if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
				 *(bti386->obufp)++ = 'q';
				 break;
				 }
				 */
				/* Fall through.  */
			case 'Q':
				if (bti386->intel_syntax && !alt)
					break;
				USED_REX (REX_W);
				if (bti386->modrm.mod != 3 || (sizeflag & SUFFIX_ALWAYS))
				{
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else
					{
						if (sizeflag & DFLAG)
							*(bti386->obufp)++ = bti386->intel_syntax ? 'd' : 'l';
						else
							*(bti386->obufp)++ = 'w';
					}
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				}
				break;
			case 'R':
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					*(bti386->obufp)++ = 'q';
				else if (sizeflag & DFLAG)
				{
					if (bti386->intel_syntax)
						*(bti386->obufp)++ = 'd';
					else
						*(bti386->obufp)++ = 'l';
				}
				else
					*(bti386->obufp)++ = 'w';
				if (bti386->intel_syntax && !p[1]
						&& ((bti386->rex & REX_W) || (sizeflag & DFLAG)))
					*(bti386->obufp)++ = 'e';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'V':
				/*
					 if (bti386->intel_syntax)
					 break;
					 if (address_mode == mode_64bit && (sizeflag & DFLAG))
					 {
					 if (sizeflag & SUFFIX_ALWAYS)
				 *(bti386->obufp)++ = 'q';
				 break;
				 }
				 */
				/* Fall through.  */
			case 'S':
				if (bti386->intel_syntax)
					break;
				if (sizeflag & SUFFIX_ALWAYS)
				{
					if (bti386->rex & REX_W)
						*(bti386->obufp)++ = 'q';
					else
					{
						if (sizeflag & DFLAG)
							*(bti386->obufp)++ = 'l';
						else
							*(bti386->obufp)++ = 'w';
						bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
					}
				}
				break;
			case 'X':
				if (bti386->prefixes & PREFIX_DATA)
					*(bti386->obufp)++ = 'd';
				else
					*(bti386->obufp)++ = 's';
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 'Y':
				/*
					 if (bti386->intel_syntax)
					 break;
					 if (bti386->rex & REX_W)
					 {
					 USED_REX (REX_W);
				 *(bti386->obufp)++ = 'q';
				 }
				 */
				break;
				/* implicit operand size 'l' for i386 or 'q' for x86-64 */
			case 'W':
				/* operand size flag for cwtl, cbtw */
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
				{
					if (bti386->intel_syntax)
						*(bti386->obufp)++ = 'd';
					else
						*(bti386->obufp)++ = 'l';
				}
				else if (sizeflag & DFLAG)
					*(bti386->obufp)++ = 'w';
				else
					*(bti386->obufp)++ = 'b';
				if (!(bti386->rex & REX_W))
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
		}
		alt = 0;
	}
	*(bti386->obufp) = 0;
	return 0;
}


	static void
oappend (bti386_t *bti386, const char *s)
{
	strlcpy ((bti386->obufp), s, ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - (bti386->obufp));
	(bti386->obufp) += strlen (s);
}

	static void
append_seg (bti386_t *bti386)
{
	if (bti386->prefixes & PREFIX_CS)
	{
		bti386->used_prefixes |= PREFIX_CS;
		oappend (bti386, "%cs:" + bti386->intel_syntax);
	}
	if (bti386->prefixes & PREFIX_DS)
	{
		bti386->used_prefixes |= PREFIX_DS;
		oappend (bti386, "%ds:" + bti386->intel_syntax);
	}
	if (bti386->prefixes & PREFIX_SS)
	{
		bti386->used_prefixes |= PREFIX_SS;
		oappend (bti386, "%ss:" + bti386->intel_syntax);
	}
	if (bti386->prefixes & PREFIX_ES)
	{
		bti386->used_prefixes |= PREFIX_ES;
		oappend (bti386, "%es:" + bti386->intel_syntax);
	}
	if (bti386->prefixes & PREFIX_FS)
	{
		bti386->used_prefixes |= PREFIX_FS;
		oappend (bti386, "%fs:" + bti386->intel_syntax);
	}
	if (bti386->prefixes & PREFIX_GS)
	{
		bti386->used_prefixes |= PREFIX_GS;
		oappend (bti386, "%gs:" + bti386->intel_syntax);
	}
}

	static void
OP_indirE (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (!bti386->intel_syntax)
		oappend (bti386, "*");
	OP_E (bti386, bytemode, sizeflag);
}

	static void
print_operand_value (bti386_t *bti386, char *buf, int buf_size, int hex, bfd_vma disp)
{
	if (address_mode == mode_64bit)
	{
		if (hex)
		{
			char tmp[30];
			int i;
			buf[0] = '0';
			buf[1] = 'x';
			snprintf (tmp, sizeof tmp, "%lx", disp);
			for (i = 0; tmp[i] == '0' && tmp[i + 1]; i++);
			strlcpy (buf + 2, tmp + i, buf_size - 2);
		}
		else
		{
			bfd_signed_vma v = disp;
			char tmp[30];
			int i;
			if (v < 0)
			{
				*(buf++) = '-';
				v = -disp;
				/* Check for possible overflow on 0x8000000000000000.  */
				if (v < 0)
				{
					strlcpy (buf, "9223372036854775808", buf_size);
					return;
				}
			}
			if (!v)
			{
				strlcpy (buf, "0", buf_size);
				return;
			}

			i = 0;
			tmp[29] = 0;
			while (v)
			{
				tmp[28 - i] = (v % 10) + '0';
				v /= 10;
				i++;
			}
			strlcpy (buf, tmp + 29 - i, buf_size);
		}
	}
	else
	{
		if (hex)
			snprintf (buf, buf_size, "0x%x", (unsigned int) disp);
		else
			snprintf (buf, buf_size, "%d", (int) disp);
	}
}

/* Put DISP in BUF as signed hex number.  */

	static void
print_displacement (bti386_t *bti386, char *buf, bfd_vma disp, size_t buf_size)
{
	bfd_signed_vma val = disp;
	char tmp[30];
	int i, j = 0;

	if (val < 0)
	{
		buf[j++] = '-';
		val = -disp;

		/* Check for possible overflow.  */
		if (val < 0)
		{
			switch (address_mode)
			{
				case mode_64bit:
					strlcpy (buf + j, "0x8000000000000000", buf_size - j);
					break;
				case mode_32bit:
					strlcpy (buf + j, "0x80000000", buf_size - j);
					break;
				case mode_16bit:
					strlcpy (buf + j, "0x8000", buf_size - j);
					break;
			}
			return;
		}
	}

	buf[j++] = '0';
	buf[j++] = 'x';

	snprintf(tmp, sizeof tmp, "%lx", val);
	for (i = 0; tmp[i] == '0'; i++)
		continue;
	if (tmp[i] == '\0')
		i--;
	strlcpy (buf + j, tmp + i, buf_size - j);
}

	static void
intel_operand_size (bti386_t *bti386, int bytemode, int sizeflag)
{
	switch (bytemode)
	{
		case b_mode:
		case dqb_mode:
			oappend (bti386, "BYTE PTR ");
			break;
		case w_mode:
		case dqw_mode:
			oappend (bti386, "WORD PTR ");
			break;
		case stack_v_mode:
			if (address_mode == mode_64bit && (sizeflag & DFLAG))
			{
				oappend (bti386, "QWORD PTR ");
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			}
			/* FALLTHRU */
		case v_mode:
		case dq_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oappend (bti386, "QWORD PTR ");
			else if ((sizeflag & DFLAG) || bytemode == dq_mode)
				oappend (bti386, "DWORD PTR ");
			else
				oappend (bti386, "WORD PTR ");
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case z_mode:
			if ((bti386->rex & REX_W) || (sizeflag & DFLAG))
				*(bti386->obufp)++ = 'D';
			oappend (bti386, "WORD PTR ");
			if (!(bti386->rex & REX_W))
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case d_mode:
		case dqd_mode:
			oappend (bti386, "DWORD PTR ");
			break;
		case q_mode:
			oappend (bti386, "QWORD PTR ");
			break;
		case m_mode:
			if (address_mode == mode_64bit)
				oappend (bti386, "QWORD PTR ");
			else
				oappend (bti386, "DWORD PTR ");
			break;
		case f_mode:
			if (sizeflag & DFLAG)
				oappend (bti386, "FWORD PTR ");
			else
				oappend (bti386, "DWORD PTR ");
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case t_mode:
			oappend (bti386, "TBYTE PTR ");
			break;
		case x_mode:
			oappend (bti386, "XMMWORD PTR ");
			break;
		case o_mode:
			oappend (bti386, "OWORD PTR ");
			break;
		default:
			break;
	}
}

	static void
OP_E (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_vma disp;
	int add = 0;
	int riprel = 0;
	USED_REX (REX_B);
	if (bti386->rex & REX_B)
		add += 8;

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;

	if (bti386->modrm.mod == 3)
	{
		switch (bytemode)
		{
			case b_mode:
				USED_REX (0);
				if (bti386->rex)
					oappend (bti386, names8rex[bti386->modrm.rm + add]);
				else
					oappend (bti386, names8[bti386->modrm.rm + add]);
				break;
			case w_mode:
				oappend (bti386, names16[bti386->modrm.rm + add]);
				break;
			case d_mode:
				oappend (bti386, names32[bti386->modrm.rm + add]);
				break;
			case q_mode:
				oappend (bti386, names64[bti386->modrm.rm + add]);
				break;
			case m_mode:
				if (address_mode == mode_64bit)
					oappend (bti386, names64[bti386->modrm.rm + add]);
				else
					oappend (bti386, names32[bti386->modrm.rm + add]);
				break;
			case stack_v_mode:
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					oappend (bti386, names64[bti386->modrm.rm + add]);
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
					break;
				}
				bytemode = v_mode;
				/* FALLTHRU */
			case v_mode:
			case dq_mode:
			case dqb_mode:
			case dqd_mode:
			case dqw_mode:
				USED_REX (REX_W);
				if (bti386->rex & REX_W)
					oappend (bti386, names64[bti386->modrm.rm + add]);
				else if ((sizeflag & DFLAG) || bytemode != v_mode)
					oappend (bti386, names32[bti386->modrm.rm + add]);
				else
					oappend (bti386, names16[bti386->modrm.rm + add]);
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 0:
				break;
			default:
				oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
				break;
		}
		return;
	}

	disp = 0;
	if (bti386->intel_syntax)
		intel_operand_size (bti386, bytemode, sizeflag);
	append_seg (bti386);

	if ((sizeflag & AFLAG) || address_mode == mode_64bit)
	{
		/* 32/64 bit address mode */
		int havedisp;
		int havesib;
		int havebase;
		int base;
		int index = 0;
		int scale = 0;

		havesib = 0;
		havebase = 1;
		base = bti386->modrm.rm;

		if (base == 4)
		{
			havesib = 1;
			index = (dis_ldub(bti386->codep) >> 3) & 7;
			if (address_mode == mode_64bit || index != 0x4)
				/* When INDEX == 0x4 in 32 bit mode, SCALE is ignored.  */
				scale = (dis_ldub(bti386->codep) >> 6) & 3;
			base = dis_ldub(bti386->codep) & 7;
			USED_REX (REX_X);
			if (bti386->rex & REX_X)
				index += 8;
			bti386->codep++;
		}
		base += add;

		switch (bti386->modrm.mod)
		{
			case 0:
				if ((base & 7) == 5)
				{
					havebase = 0;
					if (address_mode == mode_64bit && !havesib)
						riprel = 1;
					disp = get32s (bti386);
				}
				break;
			case 1:
				disp = dis_ldub(bti386->codep++);
				if ((disp & 0x80) != 0)
					disp -= 0x100;
				break;
			case 2:
				disp = get32s (bti386);
				break;
		}

		havedisp = havebase || (havesib && (index != 4 || scale != 0));

		if (!bti386->intel_syntax)
			if (bti386->modrm.mod != 0 || (base & 7) == 5)
			{
				if (havedisp || riprel)
					print_displacement (bti386, bti386->scratchbuf, disp, sizeof bti386->scratchbuf);
				else
					print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf, 1, disp);
				oappend (bti386, bti386->scratchbuf);
				if (riprel)
				{
					set_op (bti386, disp, 1);
					oappend (bti386, "(%rip)");
				}
			}

		if (havedisp || (bti386->intel_syntax && riprel))
		{
			*(bti386->obufp)++ = bti386->open_char;
			if (bti386->intel_syntax && riprel)
			{
				set_op (bti386, disp, 1);
				oappend (bti386, "rip");
			}
			*(bti386->obufp) = '\0';
			if (havebase)
				oappend (bti386, address_mode == mode_64bit && (sizeflag & AFLAG)
						? names64[base] : names32[base]);
			if (havesib)
			{
				if (index != 4)
				{
					if (!bti386->intel_syntax || havebase)
					{
						*(bti386->obufp)++ = bti386->separator_char;
						*(bti386->obufp) = '\0';
					}
					oappend (bti386, address_mode == mode_64bit && (sizeflag & AFLAG)
							? names64[index] : names32[index]);
				}
				if (scale != 0 || (!bti386->intel_syntax && index != 4))
				{
					*(bti386->obufp)++ = bti386->scale_char;
					*(bti386->obufp) = '\0';
					snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%d", 1 << scale);
					oappend (bti386, bti386->scratchbuf);
				}
			}
			if (bti386->intel_syntax
					&& (disp || bti386->modrm.mod != 0 || (base & 7) == 5))
			{
				if ((bfd_signed_vma) disp >= 0)
				{
					*(bti386->obufp)++ = '+';
					*(bti386->obufp) = '\0';
				}
				else if (bti386->modrm.mod != 1)
				{
					*(bti386->obufp)++ = '-';
					*(bti386->obufp) = '\0';
					disp = - (bfd_signed_vma) disp;
				}

				print_displacement (bti386, bti386->scratchbuf, disp, sizeof bti386->scratchbuf);
				oappend (bti386, bti386->scratchbuf);
			}

			*(bti386->obufp)++ = bti386->close_char;
			*(bti386->obufp) = '\0';
		}
		else if (bti386->intel_syntax)
		{
			if (bti386->modrm.mod != 0 || (base & 7) == 5)
			{
				if (bti386->prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
							| PREFIX_ES | PREFIX_FS | PREFIX_GS))
					;
				else
				{
					oappend (bti386, names_seg[ds_reg - es_reg]);
					oappend (bti386, ":");
				}
				print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf, 1, disp);
				oappend (bti386, bti386->scratchbuf);
			}
		}
	}
	else
	{ /* 16 bit address mode */
		switch (bti386->modrm.mod)
		{
			case 0:
				if (bti386->modrm.rm == 6)
				{
					disp = get16 (bti386);
					if ((disp & 0x8000) != 0)
						disp -= 0x10000;
				}
				break;
			case 1:
				disp = dis_ldub(bti386->codep++);
				if ((disp & 0x80) != 0)
					disp -= 0x100;
				break;
			case 2:
				disp = get16 (bti386);
				if ((disp & 0x8000) != 0)
					disp -= 0x10000;
				break;
		}

		if (!bti386->intel_syntax)
			if (bti386->modrm.mod != 0 || bti386->modrm.rm == 6)
			{
				print_displacement (bti386, bti386->scratchbuf, disp, sizeof bti386->scratchbuf);
				oappend (bti386, bti386->scratchbuf);
			}

		if (bti386->modrm.mod != 0 || bti386->modrm.rm != 6)
		{
			*(bti386->obufp)++ = bti386->open_char;
			*(bti386->obufp) = '\0';
			oappend (bti386, index16[bti386->modrm.rm]);
			if (bti386->intel_syntax
					&& (disp || bti386->modrm.mod != 0 || bti386->modrm.rm == 6))
			{
				if ((bfd_signed_vma) disp >= 0)
				{
					*(bti386->obufp)++ = '+';
					*(bti386->obufp) = '\0';
				}
				else if (bti386->modrm.mod != 1)
				{
					*(bti386->obufp)++ = '-';
					*(bti386->obufp) = '\0';
					disp = - (bfd_signed_vma) disp;
				}

				print_displacement (bti386, bti386->scratchbuf, disp, sizeof bti386->scratchbuf);
				oappend (bti386, bti386->scratchbuf);
			}

			*(bti386->obufp)++ = bti386->close_char;
			*(bti386->obufp) = '\0';
		}
		else if (bti386->intel_syntax)
		{
			if (bti386->prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
						| PREFIX_ES | PREFIX_FS | PREFIX_GS))
				;
			else
			{
				oappend (bti386, names_seg[ds_reg - es_reg]);
				oappend (bti386, ":");
			}
			print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf, 1, disp & 0xffff);
			oappend (bti386, bti386->scratchbuf);
		}
	}
}

	static void
OP_G (bti386_t *bti386, int bytemode, int sizeflag)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R)
		add += 8;
	switch (bytemode)
	{
		case b_mode:
			USED_REX (0);
			if (bti386->rex)
				oappend (bti386, names8rex[bti386->modrm.reg + add]);
			else
				oappend (bti386, names8[bti386->modrm.reg + add]);
			break;
		case w_mode:
			oappend (bti386, names16[bti386->modrm.reg + add]);
			break;
		case d_mode:
			oappend (bti386, names32[bti386->modrm.reg + add]);
			break;
		case q_mode:
			oappend (bti386, names64[bti386->modrm.reg + add]);
			break;
		case v_mode:
		case dq_mode:
		case dqb_mode:
		case dqd_mode:
		case dqw_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oappend (bti386, names64[bti386->modrm.reg + add]);
			else if ((sizeflag & DFLAG) || bytemode != v_mode)
				oappend (bti386, names32[bti386->modrm.reg + add]);
			else
				oappend (bti386, names16[bti386->modrm.reg + add]);
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case m_mode:
			if (address_mode == mode_64bit)
				oappend (bti386, names64[bti386->modrm.reg + add]);
			else
				oappend (bti386, names32[bti386->modrm.reg + add]);
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			break;
	}
}

	static bfd_vma
get64 (bti386_t *bti386)
{
	bfd_vma x;
#ifdef BFD64
	unsigned int a;
	unsigned int b;

	a = dis_ldub(bti386->codep++) & 0xff;
	a |= (dis_ldub(bti386->codep++) & 0xff) << 8;
	a |= (dis_ldub(bti386->codep++) & 0xff) << 16;
	a |= (dis_ldub(bti386->codep++) & 0xff) << 24;
	b = dis_ldub(bti386->codep++) & 0xff;
	b |= (dis_ldub(bti386->codep++) & 0xff) << 8;
	b |= (dis_ldub(bti386->codep++) & 0xff) << 16;
	b |= (dis_ldub(bti386->codep++) & 0xff) << 24;
	x = a + ((bfd_vma) b << 32);
#else
	ABORT ();
	x = 0;
#endif
	return x;
}

	static bfd_signed_vma
get32 (bti386_t *bti386)
{
	bfd_signed_vma x = 0;

	x = dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 8;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 16;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 24;
	return x;
}

	static bfd_signed_vma
get32s (bti386_t *bti386)
{
	bfd_signed_vma x = 0;

	x = dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 8;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 16;
	x |= (dis_ldub(bti386->codep++) & (bfd_signed_vma) 0xff) << 24;

	x = (x ^ ((bfd_signed_vma) 1 << 31)) - ((bfd_signed_vma) 1 << 31);

	return x;
}

	static int
get16 (bti386_t *bti386)
{
	int x = 0;

	x = dis_ldub(bti386->codep++) & 0xff;
	x |= (dis_ldub(bti386->codep++) & 0xff) << 8;
	return x;
}

	static void
set_op (bti386_t *bti386, bfd_vma op, int riprel)
{
	bti386->op_index[bti386->op_ad] = bti386->op_ad;
	if (address_mode == mode_64bit)
	{
		bti386->op_address[bti386->op_ad] = op;
		bti386->op_riprel[bti386->op_ad] = riprel;
	}
	else
	{
		/* Mask to get a 32-bit address.  */
		bti386->op_address[bti386->op_ad] = op & 0xffffffff;
		bti386->op_riprel[bti386->op_ad] = riprel & 0xffffffff;
	}
}

	static void
OP_REG (bti386_t *bti386, int code, int sizeflag)
{
	const char *s;
	int add = 0;
	USED_REX (REX_B);
	if (bti386->rex & REX_B)
		add = 8;

	switch (code)
	{
		case ax_reg: case cx_reg: case dx_reg: case bx_reg:
		case sp_reg: case bp_reg: case si_reg: case di_reg:
			s = names16[code - ax_reg + add];
			break;
		case es_reg: case ss_reg: case cs_reg:
		case ds_reg: case fs_reg: case gs_reg:
			s = names_seg[code - es_reg + add];
			break;
		case al_reg: case ah_reg: case cl_reg: case ch_reg:
		case dl_reg: case dh_reg: case bl_reg: case bh_reg:
			USED_REX (0);
			if (bti386->rex)
				s = names8rex[code - al_reg + add];
			else
				s = names8[code - al_reg];
			break;
		case rAX_reg: case rCX_reg: case rDX_reg: case rBX_reg:
		case rSP_reg: case rBP_reg: case rSI_reg: case rDI_reg:
			if (address_mode == mode_64bit && (sizeflag & DFLAG))
			{
				s = names64[code - rAX_reg + add];
				break;
			}
			code += eAX_reg - rAX_reg;
			/* Fall through.  */
		case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
		case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				s = names64[code - eAX_reg + add];
			else if (sizeflag & DFLAG)
				s = names32[code - eAX_reg + add];
			else
				s = names16[code - eAX_reg + add];
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			s = INTERNAL_DISASSEMBLER_ERROR;
			break;
	}
	oappend (bti386, s);
}

	static void
OP_IMREG (bti386_t *bti386, int code, int sizeflag)
{
	const char *s;

	switch (code)
	{
		case indir_dx_reg:
			if (bti386->intel_syntax)
				s = "dx";
			else
				s = "(%dx)";
			break;
		case ax_reg: case cx_reg: case dx_reg: case bx_reg:
		case sp_reg: case bp_reg: case si_reg: case di_reg:
			s = names16[code - ax_reg];
			break;
		case es_reg: case ss_reg: case cs_reg:
		case ds_reg: case fs_reg: case gs_reg:
			s = names_seg[code - es_reg];
			break;
		case al_reg: case ah_reg: case cl_reg: case ch_reg:
		case dl_reg: case dh_reg: case bl_reg: case bh_reg:
			USED_REX (0);
			if (bti386->rex)
				s = names8rex[code - al_reg];
			else
				s = names8[code - al_reg];
			break;
		case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
		case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				s = names64[code - eAX_reg];
			else if (sizeflag & DFLAG)
				s = names32[code - eAX_reg];
			else
				s = names16[code - eAX_reg];
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case z_mode_ax_reg:
			if ((bti386->rex & REX_W) || (sizeflag & DFLAG))
				s = *names32;
			else
				s = *names16;
			if (!(bti386->rex & REX_W))
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			s = INTERNAL_DISASSEMBLER_ERROR;
			break;
	}
	oappend (bti386, s);
}

	static void
OP_I (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_signed_vma op;
	bfd_signed_vma mask = -1;

	switch (bytemode) {
		case b_mode:
			op = dis_ldub(bti386->codep++);
			mask = 0xff;
			break;
		case q_mode:
			if (address_mode == mode_64bit) {
				op = get32s (bti386);
				break;
			}
			/* Fall through.  */
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W) {
				op = get32s (bti386);
			} else if (sizeflag & DFLAG) {
				op = get32 (bti386);
				mask = 0xffffffff;
			}
			else {
				op = get16 (bti386);
				mask = 0xfffff;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			mask = 0xfffff;
			op = get16 (bti386);
			break;
		case const_1_mode:
			if (bti386->intel_syntax)
				oappend (bti386, "1");
			return;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			return;
	}

	op &= mask;
	bti386->scratchbuf[0] = '$';
	print_operand_value (bti386, bti386->scratchbuf + 1, sizeof bti386->scratchbuf - 1, 1, op);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
	bti386->scratchbuf[0] = '\0';
}

	static void
OP_I64 (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_signed_vma op;
	bfd_signed_vma mask = -1;

	if (address_mode != mode_64bit)
	{
		OP_I (bti386, bytemode, sizeflag);
		return;
	}

	switch (bytemode)
	{
		case b_mode:
			op = dis_ldub(bti386->codep++);
			mask = 0xff;
			break;
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				op = get64 (bti386);
			else if (sizeflag & DFLAG)
			{
				op = get32 (bti386);
				mask = 0xffffffff;
			}
			else
			{
				op = get16 (bti386);
				mask = 0xfffff;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			mask = 0xfffff;
			op = get16 (bti386);
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			return;
	}

	op &= mask;
	bti386->scratchbuf[0] = '$';
	print_operand_value (bti386, bti386->scratchbuf + 1, sizeof bti386->scratchbuf - 1, 1, op);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
	bti386->scratchbuf[0] = '\0';
}

	static void
OP_sI (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_signed_vma op;
	bfd_signed_vma mask = -1;

	switch (bytemode)
	{
		case b_mode:
			op = dis_ldub(bti386->codep++);
			if ((op & 0x80) != 0)
				op -= 0x100;
			mask = 0xffffffff;
			break;
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				op = get32s (bti386);
			else if (sizeflag & DFLAG)
			{
				op = get32s (bti386);
				mask = 0xffffffff;
			}
			else
			{
				mask = 0xffffffff;
				op = get16 (bti386);
				if ((op & 0x8000) != 0)
					op -= 0x10000;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			op = get16 (bti386);
			mask = 0xffffffff;
			if ((op & 0x8000) != 0)
				op -= 0x10000;
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			return;
	}

	bti386->scratchbuf[0] = '$';
	print_operand_value (bti386, bti386->scratchbuf + 1, sizeof bti386->scratchbuf - 1, 1, op);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_J (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_vma disp;
	bfd_vma mask = -1;
	bfd_vma segment = 0;

	switch (bytemode)
	{
		case b_mode:
			disp = dis_ldub(bti386->codep++);
			if ((disp & 0x80) != 0)
				disp -= 0x100;
			break;
		case v_mode:
			if ((sizeflag & DFLAG) || (bti386->rex & REX_W))
				disp = get32s (bti386);
			else
			{
				disp = get16 (bti386);
				if ((disp & 0x8000) != 0)
					disp -= 0x10000;
				/* In 16bit mode, address is wrapped around at 64k within
					 the same segment.  Otherwise, a data16 prefix on a jump
					 instruction means that the pc is masked to 16 bits after
					 the displacement is added!  */
				mask = 0xffff;
				if ((bti386->prefixes & PREFIX_DATA) == 0)
					segment = ((bti386->start_pc + bti386->codep - bti386->start_codep)
							& ~((bfd_vma) 0xffff));
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			return;
	}
	disp = ((bti386->start_pc + bti386->codep - bti386->start_codep + disp) & mask) | segment;
	set_op (bti386, disp, 0);
	print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf - 1, 1, disp);
	oappend (bti386, bti386->scratchbuf);
}

	static void
OP_SEG (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bytemode == w_mode)
		oappend (bti386, names_seg[bti386->modrm.reg]);
	else
		OP_E (bti386, bti386->modrm.mod == 3 ? bytemode : w_mode, sizeflag);
}

	static void
OP_DIR (bti386_t *bti386, int dummy ATTRIBUTE_UNUSED, int sizeflag)
{
	int seg, offset;

	if (sizeflag & DFLAG)
	{
		offset = get32 (bti386);
		seg = get16 (bti386);
	}
	else
	{
		offset = get16 (bti386);
		seg = get16 (bti386);
	}
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	if (bti386->intel_syntax)
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "0x%x:0x%x", seg, offset);
	else
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "$0x%x,$0x%x", seg, offset);
	oappend (bti386, bti386->scratchbuf);
}

	static void
OP_OFF (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_vma off;

	if (bti386->intel_syntax && (sizeflag & SUFFIX_ALWAYS))
		intel_operand_size (bti386, bytemode, sizeflag);
	append_seg (bti386);

	if ((sizeflag & AFLAG) || address_mode == mode_64bit)
		off = get32 (bti386);
	else
		off = get16 (bti386);

	if (bti386->intel_syntax)
	{
		if (!(bti386->prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
						| PREFIX_ES | PREFIX_FS | PREFIX_GS)))
		{
			oappend (bti386, names_seg[ds_reg - es_reg]);
			oappend (bti386, ":");
		}
	}
	print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf - 1, 1, off);
	oappend (bti386, bti386->scratchbuf);
}

	static void
OP_OFF64 (bti386_t *bti386, int bytemode, int sizeflag)
{
	bfd_vma off;

	if (address_mode != mode_64bit
			|| (bti386->prefixes & PREFIX_ADDR))
	{
		OP_OFF (bti386, bytemode, sizeflag);
		return;
	}

	if (bti386->intel_syntax && (sizeflag & SUFFIX_ALWAYS))
		intel_operand_size (bti386, bytemode, sizeflag);
	append_seg (bti386);

	off = get64 (bti386);

	if (bti386->intel_syntax)
	{
		if (!(bti386->prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
						| PREFIX_ES | PREFIX_FS | PREFIX_GS)))
		{
			oappend (bti386, names_seg[ds_reg - es_reg]);
			oappend (bti386, ":");
		}
	}
	print_operand_value (bti386, bti386->scratchbuf, sizeof bti386->scratchbuf - 1, 1, off);
	oappend (bti386, bti386->scratchbuf);
}

	static void
ptr_reg (bti386_t *bti386, int code, int sizeflag)
{
	const char *s;

	*(bti386->obufp)++ = bti386->open_char;
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_ADDR);
	if (address_mode == mode_64bit) {
		if (!(sizeflag & AFLAG))
			s = names32[code - eAX_reg];
		else
			s = names64[code - eAX_reg];
	} else if (sizeflag & AFLAG) {
		s = names32[code - eAX_reg];
	} else {
		s = names16[code - eAX_reg];
	}
	oappend (bti386, s);
	*(bti386->obufp)++ = bti386->close_char;
	*(bti386->obufp) = 0;
}

	static void
OP_ESreg (bti386_t *bti386, int code, int sizeflag)
{
	if (bti386->intel_syntax)
	{
		switch (dis_ldub(bti386->codep -1))
		{
			case 0x6d:	/* insw/insl */
				intel_operand_size (bti386, z_mode, sizeflag);
				break;
			case 0xa5:	/* movsw/movsl/movsq */
			case 0xa7:	/* cmpsw/cmpsl/cmpsq */
			case 0xab:	/* stosw/stosl */
			case 0xaf:	/* scasw/scasl */
				intel_operand_size (bti386, v_mode, sizeflag);
				break;
			default:
				intel_operand_size (bti386, b_mode, sizeflag);
		}
	}
	oappend (bti386, "%es:" + bti386->intel_syntax);
	ptr_reg (bti386, code, sizeflag);
}

	static void
OP_DSreg (bti386_t *bti386, int code, int sizeflag)
{
	if (bti386->intel_syntax)
	{
		switch (dis_ldub(bti386->codep -1))
		{
			case 0x6f:	/* outsw/outsl */
				intel_operand_size (bti386, z_mode, sizeflag);
				break;
			case 0xa5:	/* movsw/movsl/movsq */
			case 0xa7:	/* cmpsw/cmpsl/cmpsq */
			case 0xad:	/* lodsw/lodsl/lodsq */
				intel_operand_size (bti386, v_mode, sizeflag);
				break;
			default:
				intel_operand_size (bti386, b_mode, sizeflag);
		}
	}
	if ((bti386->prefixes
				& (PREFIX_CS
					| PREFIX_DS
					| PREFIX_SS
					| PREFIX_ES
					| PREFIX_FS
					| PREFIX_GS)) == 0)
		bti386->prefixes |= PREFIX_DS;
	append_seg (bti386);
	ptr_reg (bti386, code, sizeflag);
}

	static void
OP_C (bti386_t *bti386, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	if (bti386->rex & REX_R)
	{
		USED_REX (REX_R);
		add = 8;
	}
	else if (address_mode != mode_64bit && (bti386->prefixes & PREFIX_LOCK))
	{
		bti386->used_prefixes |= PREFIX_LOCK;
		add = 8;
	}
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%cr%d", bti386->modrm.reg + add);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_D (bti386_t *bti386, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R)
		add = 8;
	if (bti386->intel_syntax)
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "db%d", bti386->modrm.reg + add);
	else
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%db%d", bti386->modrm.reg + add);
	oappend (bti386, bti386->scratchbuf);
}

	static void
OP_T (bti386_t *bti386, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%tr%d", bti386->modrm.reg);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_R (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3)
		OP_E (bti386, bytemode, sizeflag);
	else
		BadOp (bti386);
}

	static void
OP_MMX (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	if (bti386->prefixes & PREFIX_DATA) {
		int add = 0;
		USED_REX (REX_R);
		if (bti386->rex & REX_R)
			add = 8;
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%xmm%d", bti386->modrm.reg + add);
	}
	else {
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%mm%d", bti386->modrm.reg);
	}
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_XMM (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R)
		add = 8;
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%xmm%d", bti386->modrm.reg + add);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_EM (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod != 3)
	{
		if (bti386->intel_syntax && bytemode == v_mode)
		{
			bytemode = (bti386->prefixes & PREFIX_DATA) ? x_mode : q_mode;
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
		}
		OP_E (bti386, bytemode, sizeflag);
		return;
	}

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	if (bti386->prefixes & PREFIX_DATA)
	{
		int add = 0;

		USED_REX (REX_B);
		if (bti386->rex & REX_B)
			add = 8;
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%xmm%d", bti386->modrm.rm + add);
	}
	else
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%mm%d", bti386->modrm.rm);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

/* cvt* are the only instructions in sse2 which have
	 both SSE and MMX operands and also have 0x66 prefix
	 in their opcode. 0x66 was originally used to differentiate
	 between SSE and MMX instruction(operands). So we have to handle the
	 cvt* separately using OP_EMC and OP_MXC */
	static void
OP_EMC (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod != 3)
	{
		if (bti386->intel_syntax && bytemode == v_mode)
		{
			bytemode = (bti386->prefixes & PREFIX_DATA) ? x_mode : q_mode;
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
		}
		OP_E (bti386, bytemode, sizeflag);
		return;
	}

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%mm%d", bti386->modrm.rm);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_MXC (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%mm%d", bti386->modrm.reg);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_EX (bti386_t *bti386, int bytemode, int sizeflag)
{
	int add = 0;
	if (bti386->modrm.mod != 3)
	{
		OP_E (bti386, bytemode, sizeflag);
		return;
	}
	USED_REX (REX_B);
	if (bti386->rex & REX_B)
		add = 8;

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%xmm%d", bti386->modrm.rm + add);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
OP_MS (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3)
		OP_EM (bti386, bytemode, sizeflag);
	else
		BadOp (bti386);
}

	static void
OP_XS (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3)
		OP_EX (bti386, bytemode, sizeflag);
	else
		BadOp (bti386);
}

	static void
OP_M (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3)
		/* bad bound,lea,lds,les,lfs,lgs,lss,cmpxchg8b,vmptrst modrm */
		BadOp (bti386);
	else
		OP_E (bti386, bytemode, sizeflag);
}

	static void
OP_0f07 (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod != 3 || bti386->modrm.rm != 0)
		BadOp (bti386);
	else
		OP_E (bti386, bytemode, sizeflag);
}

	static void
OP_0fae (bti386_t *bti386, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3)
	{
		if (bti386->modrm.reg == 7) {
			char *ptr = bti386->obuf + strlen (bti386->obuf) - sizeof ("clflush") + 1;
			strlcpy (ptr, "sfence", ((char *)bti386->obuf + sizeof bti386->obuf) - ptr);
		}

		if (bti386->modrm.reg < 5 || bti386->modrm.rm != 0)
		{
			//BadOp (bti386);	/* bad sfence, mfence, or lfence */
			ASSERT(0);
			return;
		}
	}
	else if (bti386->modrm.reg != 7)
	{
		//BadOp (bti386);		/* bad clflush */
		ASSERT(0);
		return;
	}

	OP_E (bti386, bytemode, sizeflag);
}

/* NOP is an alias of "xchg %ax,%ax" in 16bit mode, "xchg %eax,%eax" in
	 32bit mode and "xchg %rax,%rax" in 64bit mode.  */

	static void
NOP_Fixup1 (bti386_t *bti386, int bytemode, int sizeflag)
{
	if ((bti386->prefixes & PREFIX_DATA) != 0
			|| (bti386->rex != 0
				&& bti386->rex != 0x48
				&& address_mode == mode_64bit))
		OP_REG (bti386, bytemode, sizeflag);
	else
		strlcpy (bti386->obuf, "nop", sizeof bti386->obuf);
}

	static void
NOP_Fixup2 (bti386_t *bti386, int bytemode, int sizeflag)
{
	if ((bti386->prefixes & PREFIX_DATA) != 0
			|| (bti386->rex != 0
				&& bti386->rex != 0x48
				&& address_mode == mode_64bit))
		OP_IMREG (bti386, bytemode, sizeflag);
}

static const char *const Suffix3DNow[] = {
	/* 00 */	NULL,		NULL,		NULL,		NULL,
	/* 04 */	NULL,		NULL,		NULL,		NULL,
	/* 08 */	NULL,		NULL,		NULL,		NULL,
	/* 0C */	"pi2fw",	"pi2fd",	NULL,		NULL,
	/* 10 */	NULL,		NULL,		NULL,		NULL,
	/* 14 */	NULL,		NULL,		NULL,		NULL,
	/* 18 */	NULL,		NULL,		NULL,		NULL,
	/* 1C */	"pf2iw",	"pf2id",	NULL,		NULL,
	/* 20 */	NULL,		NULL,		NULL,		NULL,
	/* 24 */	NULL,		NULL,		NULL,		NULL,
	/* 28 */	NULL,		NULL,		NULL,		NULL,
	/* 2C */	NULL,		NULL,		NULL,		NULL,
	/* 30 */	NULL,		NULL,		NULL,		NULL,
	/* 34 */	NULL,		NULL,		NULL,		NULL,
	/* 38 */	NULL,		NULL,		NULL,		NULL,
	/* 3C */	NULL,		NULL,		NULL,		NULL,
	/* 40 */	NULL,		NULL,		NULL,		NULL,
	/* 44 */	NULL,		NULL,		NULL,		NULL,
	/* 48 */	NULL,		NULL,		NULL,		NULL,
	/* 4C */	NULL,		NULL,		NULL,		NULL,
	/* 50 */	NULL,		NULL,		NULL,		NULL,
	/* 54 */	NULL,		NULL,		NULL,		NULL,
	/* 58 */	NULL,		NULL,		NULL,		NULL,
	/* 5C */	NULL,		NULL,		NULL,		NULL,
	/* 60 */	NULL,		NULL,		NULL,		NULL,
	/* 64 */	NULL,		NULL,		NULL,		NULL,
	/* 68 */	NULL,		NULL,		NULL,		NULL,
	/* 6C */	NULL,		NULL,		NULL,		NULL,
	/* 70 */	NULL,		NULL,		NULL,		NULL,
	/* 74 */	NULL,		NULL,		NULL,		NULL,
	/* 78 */	NULL,		NULL,		NULL,		NULL,
	/* 7C */	NULL,		NULL,		NULL,		NULL,
	/* 80 */	NULL,		NULL,		NULL,		NULL,
	/* 84 */	NULL,		NULL,		NULL,		NULL,
	/* 88 */	NULL,		NULL,		"pfnacc",	NULL,
	/* 8C */	NULL,		NULL,		"pfpnacc",	NULL,
	/* 90 */	"pfcmpge",	NULL,		NULL,		NULL,
	/* 94 */	"pfmin",	NULL,		"pfrcp",	"pfrsqrt",
	/* 98 */	NULL,		NULL,		"pfsub",	NULL,
	/* 9C */	NULL,		NULL,		"pfadd",	NULL,
	/* A0 */	"pfcmpgt",	NULL,		NULL,		NULL,
	/* A4 */	"pfmax",	NULL,		"pfrcpit1",	"pfrsqit1",
	/* A8 */	NULL,		NULL,		"pfsubr",	NULL,
	/* AC */	NULL,		NULL,		"pfacc",	NULL,
	/* B0 */	"pfcmpeq",	NULL,		NULL,		NULL,
	/* B4 */	"pfmul",	NULL,		"pfrcpit2",	"pmulhrw",
	/* B8 */	NULL,		NULL,		NULL,		"pswapd",
	/* BC */	NULL,		NULL,		NULL,		"pavgusb",
	/* C0 */	NULL,		NULL,		NULL,		NULL,
	/* C4 */	NULL,		NULL,		NULL,		NULL,
	/* C8 */	NULL,		NULL,		NULL,		NULL,
	/* CC */	NULL,		NULL,		NULL,		NULL,
	/* D0 */	NULL,		NULL,		NULL,		NULL,
	/* D4 */	NULL,		NULL,		NULL,		NULL,
	/* D8 */	NULL,		NULL,		NULL,		NULL,
	/* DC */	NULL,		NULL,		NULL,		NULL,
	/* E0 */	NULL,		NULL,		NULL,		NULL,
	/* E4 */	NULL,		NULL,		NULL,		NULL,
	/* E8 */	NULL,		NULL,		NULL,		NULL,
	/* EC */	NULL,		NULL,		NULL,		NULL,
	/* F0 */	NULL,		NULL,		NULL,		NULL,
	/* F4 */	NULL,		NULL,		NULL,		NULL,
	/* F8 */	NULL,		NULL,		NULL,		NULL,
	/* FC */	NULL,		NULL,		NULL,		NULL,
};

	static void
OP_3DNowSuffix (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	const char *mnemonic;

	/* AMD 3DNow! instructions are specified by an opcode suffix in the
		 place where an 8-bit immediate would normally go.  ie. the last
		 byte of the instruction.  */
	(bti386->obufp) = (bti386->obuf) + strlen ((bti386->obuf));
	mnemonic = Suffix3DNow[dis_ldub(bti386->codep++) & 0xff];
	if (mnemonic)
		oappend (bti386, mnemonic);
	else
	{
		/* Since a variable sized modrm/sib chunk is between the start
			 of the opcode (0x0f0f) and the opcode suffix, we need to do
			 all the modrm processing first, and don't know until now that
			 we have a bad opcode.  This necessitates some cleaning up.  */
		bti386->op_out[0][0] = '\0';
		bti386->op_out[1][0] = '\0';
		BadOp (bti386);
	}
}

static const char *simd_cmp_op[] = {
	"eq",
	"lt",
	"le",
	"unord",
	"neq",
	"nlt",
	"nle",
	"ord"
};

	static void
OP_SIMD_Suffix (bti386_t *bti386, int bytemode ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	unsigned int cmp_type;

	(bti386->obufp) = bti386->obuf + strlen ((bti386->obuf));
	cmp_type = dis_ldub(bti386->codep++) & 0xff;
	if (cmp_type < 8) {
		char suffix1 = 'p', suffix2 = 's';
		bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
		if (bti386->prefixes & PREFIX_REPZ)
			suffix1 = 's';
		else {
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			if (bti386->prefixes & PREFIX_DATA)
				suffix2 = 'd';
			else
			{
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPNZ);
				if (bti386->prefixes & PREFIX_REPNZ)
					suffix1 = 's', suffix2 = 'd';
			}
		}
		snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "cmp%s%c%c",
				simd_cmp_op[cmp_type], suffix1, suffix2);
		bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
		oappend (bti386, bti386->scratchbuf);
	} else {
		/* We have a bad extension byte.  Clean up.  */
		bti386->op_out[0][0] = '\0';
		bti386->op_out[1][0] = '\0';
		BadOp (bti386);
	}
}

	static void
SIMD_Fixup (bti386_t *bti386, int extrachar, int sizeflag ATTRIBUTE_UNUSED)
{
	/* Change movlps/movhps to movhlps/movlhps for 2 register operand
		 forms of these instructions.  */
	if (bti386->modrm.mod == 3)
	{
		char *p = (bti386->obuf) + strlen ((bti386->obuf));
		*(p + 1) = '\0';
		*p       = *(p - 1);
		*(p - 1) = *(p - 2);
		*(p - 2) = *(p - 3);
		*(p - 3) = extrachar;
	}
}

	static void
PNI_Fixup (bti386_t *bti386, int extrachar ATTRIBUTE_UNUSED, int sizeflag)
{
	if (bti386->modrm.mod == 3 && bti386->modrm.reg == 1 && bti386->modrm.rm <= 1)
	{
		/* Override "sidt".  */
		size_t olen = strlen ((bti386->obuf));
		char *p = (bti386->obuf) + olen - 4;
		const char **names = (address_mode == mode_64bit
				? names64 : names32);

		/* We might have a suffix when disassembling with -Msuffix.  */
		if (*p == 'i')
			--p;

		/* Remove "addr16/addr32" if we aren't in Intel mode.  */
		if (!bti386->intel_syntax
				&& (bti386->prefixes & PREFIX_ADDR)
				&& olen >= (4 + 7)
				&& *(p - 1) == ' '
				&& CONST_STRNEQ (p - 7, "addr")
				&& (CONST_STRNEQ (p - 3, "16")
					|| CONST_STRNEQ (p - 3, "32")))
			p -= 7;

		if (bti386->modrm.rm)
		{
			/* mwait %eax,%ecx  */
			strlcpy (p, "mwait", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
			if (!bti386->intel_syntax)
				strlcpy (bti386->op_out[0], names[0], sizeof bti386->op_out[0]);
		}
		else
		{
			/* monitor %eax,%ecx,%edx"  */
			strlcpy (p, "monitor", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
			if (!bti386->intel_syntax)
			{
				const char **op1_names;
				if (!(bti386->prefixes & PREFIX_ADDR))
					op1_names = (address_mode == mode_16bit
							? names16 : names);
				else
				{
					op1_names = (address_mode != mode_32bit
							? names32 : names16);
					bti386->used_prefixes |= PREFIX_ADDR;
				}
				strlcpy (bti386->op_out[0], op1_names[0], sizeof bti386->op_out[0]);
				strlcpy (bti386->op_out[2], names[2], sizeof bti386->op_out[2]);
			}
		}
		if (!bti386->intel_syntax)
		{
			strlcpy (bti386->op_out[1], names[1], sizeof bti386->op_out[1]);
			bti386->two_source_ops = 1;
		}

		bti386->codep++;
	}
	else
		OP_M (bti386, 0, sizeflag);
}

	static void
SVME_Fixup (bti386_t *bti386, int bytemode, int sizeflag)
{
	const char *alt;
	char *p;

	switch (dis_ldub(bti386->codep))
	{
		case 0xd8:
			alt = "vmrun";
			break;
		case 0xd9:
			alt = "vmmcall";
			break;
		case 0xda:
			alt = "vmload";
			break;
		case 0xdb:
			alt = "vmsave";
			break;
		case 0xdc:
			alt = "stgi";
			break;
		case 0xdd:
			alt = "clgi";
			break;
		case 0xde:
			alt = "skinit";
			break;
		case 0xdf:
			alt = "invlpga";
			break;
		default:
			OP_M (bti386, bytemode, sizeflag);
			return;
	}
	/* Override "lidt".  */
	p = (bti386->obuf) + strlen ((bti386->obuf)) - 4;
	/* We might have a suffix.  */
	if (*p == 'i')
		--p;
	strlcpy (p, alt, ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
	if (!(bti386->prefixes & PREFIX_ADDR))
	{
		++bti386->codep;
		return;
	}
	bti386->used_prefixes |= PREFIX_ADDR;
	switch (dis_ldub(bti386->codep++))
	{
		case 0xdf:
			strlcpy (bti386->op_out[1], names32[1], sizeof bti386->op_out[1]);
			bti386->two_source_ops = 1;
			/* Fall through.  */
		case 0xd8:
		case 0xda:
		case 0xdb:
			*(bti386->obufp)++ = bti386->open_char;
			if (address_mode == mode_64bit || (sizeflag & AFLAG))
				alt = names32[0];
			else
				alt = names16[0];
			strlcpy ((bti386->obufp), alt, ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - (bti386->obufp));
			(bti386->obufp) += strlen (alt);
			*(bti386->obufp)++ = bti386->close_char;
			*(bti386->obufp) = '\0';
			break;
	}
}

	static void
INVLPG_Fixup (bti386_t *bti386, int bytemode, int sizeflag)
{
	const char *alt;

	switch (dis_ldub(bti386->codep))
	{
		case 0xf8:
			alt = "swapgs";
			break;
		case 0xf9:
			alt = "rdtscp";
			break;
		default:
			OP_M (bti386, bytemode, sizeflag);
			return;
	}
	/* Override "invlpg".  */
	strlcpy ((bti386->obuf) + strlen ((bti386->obuf)) - 6, alt, sizeof (bti386->obuf) - (strlen((bti386->obuf)) - 6));
	bti386->codep++;
}

	static void
BadOp (bti386_t *bti386)
{
	/* Throw away prefixes and 1st. opcode byte.  */
	bti386->codep = bti386->insn_codep + 1;
	oappend (bti386, "(bad)");
}

	static void
VMX_Fixup (bti386_t *bti386, int extrachar ATTRIBUTE_UNUSED, int sizeflag)
{
	if (bti386->modrm.mod == 3
			&& bti386->modrm.reg == 0
			&& bti386->modrm.rm >=1
			&& bti386->modrm.rm <= 4)
	{
		/* Override "sgdt".  */
		char *p = (bti386->obuf) + strlen ((bti386->obuf)) - 4;

		/* We might have a suffix when disassembling with -Msuffix.  */
		if (*p == 'g')
			--p;

		switch (bti386->modrm.rm)
		{
			case 1:
				strlcpy (p, "vmcall", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				break;
			case 2:
				strlcpy (p, "vmlaunch", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				break;
			case 3:
				strlcpy (p, "vmresume", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				break;
			case 4:
				strlcpy (p, "vmxoff", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				break;
		}

		bti386->codep++;
	}
	else
		OP_E (bti386, 0, sizeflag);
}

	static void
OP_VMX (bti386_t *bti386, int bytemode, int sizeflag)
{
	bti386->used_prefixes |= (bti386->prefixes & (PREFIX_DATA | PREFIX_REPZ));
	if (bti386->prefixes & PREFIX_DATA)
		strlcpy ((bti386->obuf), "vmclear", sizeof (bti386->obuf));
	else if (bti386->prefixes & PREFIX_REPZ)
		strlcpy ((bti386->obuf), "vmxon", sizeof (bti386->obuf));
	else
		strlcpy ((bti386->obuf), "vmptrld", sizeof (bti386->obuf));
	OP_E (bti386, bytemode, sizeflag);
}

	static void
REP_Fixup (bti386_t *bti386, int bytemode, int sizeflag)
{
	/* The 0xf3 prefix should be displayed as "rep" for ins, outs, movs,
		 lods and stos.  */
	size_t ilen = 0;

	if (bti386->prefixes & PREFIX_REPZ)
		switch (dis_ldub(bti386->insn_codep))
		{
			case 0x6e:	/* outsb */
			case 0x6f:	/* outsw/outsl */
			case 0xa4:	/* movsb */
			case 0xa5:	/* movsw/movsl/movsq */
				if (!bti386->intel_syntax)
					ilen = 5;
				else
					ilen = 4;
				break;
			case 0xaa:	/* stosb */
			case 0xab:	/* stosw/stosl/stosq */
			case 0xac:	/* lodsb */
			case 0xad:	/* lodsw/lodsl/lodsq */
				if (!bti386->intel_syntax && (sizeflag & SUFFIX_ALWAYS))
					ilen = 5;
				else
					ilen = 4;
				break;
			case 0x6c:	/* insb */
			case 0x6d:	/* insl/insw */
				if (!bti386->intel_syntax)
					ilen = 4;
				else
					ilen = 3;
				break;
			default:
				ABORT ();
				break;
		}

	if (ilen != 0)
	{
		size_t olen;
		char *p;

		olen = strlen ((bti386->obuf));
		p = (bti386->obuf) + olen - ilen - 1 - 4;
		/* Handle "repz [addr16|addr32]".  */
		if ((bti386->prefixes & PREFIX_ADDR))
			p -= 1 + 6;

		memmove (p + 3, p + 4, olen - (p + 3 - (bti386->obuf)));
	}

	switch (bytemode)
	{
		case al_reg:
		case eAX_reg:
		case indir_dx_reg:
			OP_IMREG (bti386, bytemode, sizeflag);
			break;
		case eDI_reg:
			OP_ESreg (bti386, bytemode, sizeflag);
			break;
		case eSI_reg:
			OP_DSreg (bti386, bytemode, sizeflag);
			break;
		default:
			ABORT ();
			break;
	}
}

	static void
Prefix(bti386_t *bti386, int bytemode, int sizeflag)
{
}

	static void
CMPXCHG8B_Fixup (bti386_t *bti386, int bytemode, int sizeflag)
{
	USED_REX (REX_W);
	if (bti386->rex & REX_W)
	{
		/* Change cmpxchg8b to cmpxchg16b.  */
		char *p = (bti386->obuf) + strlen ((bti386->obuf)) - 2;
		strlcpy (p, "16b", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
		bytemode = o_mode;
	}
	OP_M (bti386, bytemode, sizeflag);
}

	static void
XMM_Fixup (bti386_t *bti386, int reg, int sizeflag ATTRIBUTE_UNUSED)
{
	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%xmm%d", reg);
	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
CRC32_Fixup (bti386_t *bti386, int bytemode, int sizeflag)
{
	/* Add proper suffix to "crc32".  */
	char *p = (bti386->obuf) + strlen ((bti386->obuf));

	switch (bytemode)
	{
		case b_mode:
			if (bti386->intel_syntax)
				break;

			*p++ = 'b';
			break;
		case v_mode:
			if (bti386->intel_syntax)
				break;

			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				*p++ = 'q';
			else if (sizeflag & DFLAG)
				*p++ = 'l';
			else
				*p++ = 'w';
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			break;
	}
	*p = '\0';

	if (bti386->modrm.mod == 3)
	{
		int add;

		/* Skip mod/rm byte.  */
		MODRM_CHECK;
		bti386->codep++;

		USED_REX (REX_B);
		add = (bti386->rex & REX_B) ? 8 : 0;
		if (bytemode == b_mode)
		{
			USED_REX (0);
			if (bti386->rex)
				oappend (bti386, names8rex[bti386->modrm.rm + add]);
			else
				oappend (bti386, names8[bti386->modrm.rm + add]);
		}
		else
		{
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oappend (bti386, names64[bti386->modrm.rm + add]);
			else if ((bti386->prefixes & PREFIX_DATA))
				oappend (bti386, names16[bti386->modrm.rm + add]);
			else
				oappend (bti386, names32[bti386->modrm.rm + add]);
		}
	}
	else
		OP_E (bti386, bytemode, sizeflag);
}

	static void
DIS_CRC32_Fixup (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	/* Add proper suffix to "crc32".  */
	char *p = bti386->obuf + strlen (bti386->obuf);

	switch (bytemode)
	{
		case b_mode:
			if (bti386->intel_syntax)
				break;

			*p++ = 'b';
			break;
		case v_mode:
			if (bti386->intel_syntax)
				break;

			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				*p++ = 'q';
			else if (sizeflag & DFLAG)
				*p++ = 'l';
			else
				*p++ = 'w';
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			break;
	}
	*p = '\0';

	if (bti386->modrm.mod == 3)
	{
		int add;

		/* Skip mod/rm byte.  */
		MODRM_CHECK;
		bti386->codep++;

		USED_REX (REX_B);
		add = (bti386->rex & REX_B) ? 8 : 0;
		if (bytemode == b_mode)
		{
			USED_REX (0);
			if (bti386->rex) {
				op->size = 1;
				//oappend (names8rex[modrm.rm + add]);
			} else {
				op->size = 1;
				//oappend (names8[modrm.rm + add]);
			}
		}
		else
		{
			USED_REX (REX_W);
			if (bti386->rex & REX_W) {
				op->size = 8;
				//oappend (names64[modrm.rm + add]);
			} else if ((bti386->prefixes & PREFIX_DATA)) {
				op->size = 2;
				//oappend (names16[modrm.rm + add]);
			} else {
				op->size = 4;
				//oappend (names32[modrm.rm + add]);
			}
		}
	}
	else
		DIS_E (bti386, op, bytemode, sizeflag);
}

int
disas_insn(btmod_vcpu *v, unsigned char const *code, target_ulong eip, 
		insn_t *insn, unsigned size, bool guest)
{
	const struct dis386 *dp;
	unsigned char uses_DATA_prefix, uses_LOCK_prefix;
	unsigned char uses_REPNZ_prefix, uses_REPZ_prefix;
	int sizeflag = DFLAG | AFLAG;
	unsigned char op = 0;
	unsigned char b;
	btmod *bt = v->bt;
	bti386_t *bti386 = &v->bti386;

	guest_flag = guest;
	insn_init(insn);
	bti386->start_codep = (void *)(target_ulong)(code - eip);
	bti386->codep = code;
	bti386->obufp = (bti386->obuf);
	ckprefix(bti386);
	bti386->insn_codep = bti386->codep;

	switch (size) {
		case 2: sizeflag = 0;
						break;
		case 4: sizeflag = DFLAG | AFLAG;
						break;
		default: ASSERT(0);
						 break;
	}
	b = dis_ldub(bti386->codep);
	if (b == 0x0f) {
		unsigned char threebyte;
		threebyte = dis_ldub(++bti386->codep);
		dp = &dis386_twobyte[threebyte];
		(bti386->need_modrm) = twobyte_has_modrm[dis_ldub(bti386->codep)];
		uses_DATA_prefix = twobyte_uses_DATA_prefix[dis_ldub(bti386->codep)];
		uses_REPNZ_prefix = twobyte_uses_REPNZ_prefix[dis_ldub(bti386->codep)];
		uses_REPZ_prefix = twobyte_uses_REPZ_prefix[dis_ldub(bti386->codep)];
		uses_LOCK_prefix = (dis_ldub(bti386->codep) & ~0x02) == 0x20;
		bti386->codep++;
		if (dp->name == NULL && dp->op[0].bytemode == IS_3BYTE_OPCODE) {
			op = dis_ldub(bti386->codep++);
			switch (threebyte) {
				case 0x38:
					uses_DATA_prefix = threebyte_0x38_uses_DATA_prefix[op];
					uses_REPNZ_prefix = threebyte_0x38_uses_REPNZ_prefix[op];
					uses_REPZ_prefix = threebyte_0x38_uses_REPZ_prefix[op];
					break;
				case 0x3a:
					uses_DATA_prefix = threebyte_0x3a_uses_DATA_prefix[op];
					uses_REPNZ_prefix = threebyte_0x3a_uses_REPNZ_prefix[op];
					uses_REPZ_prefix = threebyte_0x3a_uses_REPZ_prefix[op];
					break;
				default:
					break;
			}
		}
	} else {
		dp = &dis386[b];
		(bti386->need_modrm) = onebyte_has_modrm[dis_ldub(bti386->codep)];
		uses_DATA_prefix = 0;
		uses_REPNZ_prefix = 0;
		/* pause is 0xf3 0x90.  */
		uses_REPZ_prefix = dis_ldub(bti386->codep) == 0x90;
		uses_LOCK_prefix = 0;
		bti386->codep++;
	}

	if (!uses_REPZ_prefix && (bti386->prefixes & PREFIX_REPZ)) {
		bti386->used_prefixes |= PREFIX_REPZ;
	}
	if (!uses_REPNZ_prefix && (bti386->prefixes & PREFIX_REPNZ)) {
		bti386->used_prefixes |= PREFIX_REPNZ;
	}

	if (!uses_LOCK_prefix && (bti386->prefixes & PREFIX_LOCK)) {
		bti386->used_prefixes |= PREFIX_LOCK;
	}

	if (bti386->prefixes & PREFIX_ADDR) {
		sizeflag ^= AFLAG;
		if (dp->op[2].bytemode != loop_jcxz_mode || bti386->intel_syntax) {
			if ((sizeflag & AFLAG) || address_mode == mode_64bit) {
				oappend (bti386, "addr32 ");
			} else {
				oappend (bti386, "addr16 ");
			}
			bti386->used_prefixes |= PREFIX_ADDR;
		}
	}

	if (!uses_DATA_prefix && (bti386->prefixes & PREFIX_DATA)) {
		sizeflag ^= DFLAG;
		if (dp->op[2].bytemode == cond_jump_mode
				&& dp->op[0].bytemode == v_mode
				&& !bti386->intel_syntax) {
			if (sizeflag & DFLAG) {
				oappend (bti386, "data32 ");
			} else {
				oappend (bti386, "data16 ");
			}
			bti386->used_prefixes |= PREFIX_DATA;
		}
	}

	if (dp->name == NULL && dp->op[0].bytemode == IS_3BYTE_OPCODE) {
		dp = &three_byte_table[dp->op[1].bytemode][op];
		bti386->modrm.mod = (dis_ldub(bti386->codep) >> 6) & 3;
		bti386->modrm.reg = (dis_ldub(bti386->codep) >> 3) & 7;
		bti386->modrm.rm = dis_ldub(bti386->codep) & 7;
	} else if ((bti386->need_modrm)) {
		bti386->modrm.mod = (dis_ldub(bti386->codep) >> 6) & 3;
		bti386->modrm.reg = (dis_ldub(bti386->codep) >> 3) & 7;
		bti386->modrm.rm = dis_ldub(bti386->codep) & 7;
	}

	if (dp->name == NULL && dp->op[0].bytemode == FLOATCODE) {
		disas_dofloat (v, insn, sizeflag);
		if (insn->opc == -1) {
			return 0;
		}
		return bti386->codep - code;
	} else {
		int index;
		if (dp->name == NULL) {
			switch (dp->op[0].bytemode) {
				case USE_GROUPS:
					dp = &grps[dp->op[1].bytemode][bti386->modrm.reg];
					break;

				case USE_PREFIX_USER_TABLE:
					index = 0;
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
					if (bti386->prefixes & PREFIX_REPZ)
						index = 1;
					else {
						/* We should check PREFIX_REPNZ and PREFIX_REPZ
							 before PREFIX_DATA.  */
						bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPNZ);
						if (bti386->prefixes & PREFIX_REPNZ)
							index = 3;
						else {
							bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
							if (bti386->prefixes & PREFIX_DATA)
								index = 2;
						}
					}
					dp = &prefix_user_table[dp->op[1].bytemode][index];
					break;

				case X86_64_SPECIAL:
					index = address_mode == mode_64bit ? 1 : 0;
					dp = &x86_64_table[dp->op[1].bytemode][index];
					break;

				default:
					//oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
					break;
			}
		}
		insn->opc = opctable_find(bt, dp->dpnum, sizeflag);
		/*if (insn->opc == -1) {
			int i;
			for (i = 0; i < 20; i++) {
				dprintk("%x ", code[i]);
			}
			dprintk("\n");
		}*/
		ASSERT(insn->opc != -1);
		/*if (insn->opc == -1) {
			DBG(bti386, I386_DISAS, "%s", "Returning 0 because opc is -1\n");
			return 0;
		}*/
		bti386->rename_opcode = NULL;
		if (dp->dpnum != dp_inval) {
			int i;
			for (i = 0; i < MAX_NUM_OPERANDS; i++) {
				if ((dp->op[i]).disas_rtn) {
					(*dp->op[i].disas_rtn)(bti386, &insn->op[i], dp->op[i].bytemode, sizeflag);
				} else {
					insn->op[i].type = invalid;
				}
			}
			if (bti386->rename_opcode) {
				if (!strcmp(bti386->rename_opcode, "(bad)")) {
				  return 0;
				}
				//insn->opc = opctable_find(bt, bti386->rename_opcode);
		  }
			return bti386->codep - code;
		}
	}

	return 0;
}

	static void
opctable_dp(btmod_vcpu *v, struct dis386 const *dpc)
{
	//static int dpnum = dp_inval;
	btmod *bt = v->bt;
	bti386_t *bti386 = &v->bti386;
	int sizeflags[] = {0, AFLAG, DFLAG, DFLAG | AFLAG};
	size_t num_sizeflags = sizeof sizeflags/sizeof sizeflags[0];
	struct dis386 *dp = (struct dis386 *)dpc;

	if (dp->name != NULL) {
		char buf[32];
		unsigned i;
		dp->dpnum = ++(bt->ot.dpnum);

		for (i = 0; i < num_sizeflags; i++) {
			int sizeflag = sizeflags[i];
			bti386->obufp = buf;
			disas_putop(bti386, dp->name, sizeflag);
			opctable_insert(bt, buf, dp->dpnum, sizeflag);
		}
	} else {
		dp->dpnum = dp_inval;
	}
}

	static void
opctable_float_mem(btmod_vcpu *v, char const *fm, int *fm_dpnum)
{
	struct dis386 tmp_dp;
	tmp_dp.name = fm;
	opctable_dp(v, &tmp_dp);
	*fm_dpnum = tmp_dp.dpnum;
}

	static void
opctable_fgrps(btmod_vcpu *v, char *fg, int *fg_dpnum)
{
	struct dis386 tmp_dp;
	tmp_dp.name = fg;
	opctable_dp(v, &tmp_dp);
	*fg_dpnum = tmp_dp.dpnum;
}

	static void
opctable_fill(btmod_vcpu *v, struct dis386 const *dptab, size_t dptab_size)
{
	struct dis386 const *dp, *fdp;
	int opcnum = 0;

	for (dp = dptab; dp < dptab + dptab_size; dp++) {
		opcnum++;
		if (dp->name == NULL) {
			if (dp->op[0].bytemode == IS_3BYTE_OPCODE) {
				opctable_fill(v, three_byte_table[dp->op[1].bytemode], 256);
			} else if (dp->op[0].bytemode == FLOATCODE) {
				int floatop;
				for (floatop = 0; floatop < 8; floatop++) {
					int op;
					for (op = 0; op < 8; op++) {
						fdp = &float_reg[floatop][op];
						if (fdp->name == NULL) {
							int rm;
							for (rm = 0; rm < 8; rm++) {
								opctable_fgrps(v, fgrps[fdp->op[0].bytemode][rm],
										&fgrps_dpnum[fdp->op[0].bytemode][rm]);
							}
						} else {
							opctable_dp(v, fdp);
						}
					}
				}
			} else {
				int reg, index;
				switch (dp->op[0].bytemode) {
					case USE_GROUPS:
						for (reg = 0; reg < 8; reg++) {
							opctable_dp(v, &grps[dp->op[1].bytemode][reg]);
						}
						break;
					case USE_PREFIX_USER_TABLE:
						for (index = 0; index < 4; index++) {
							opctable_dp(v, &prefix_user_table[dp->op[1].bytemode][index]);
						}
						break;
					case X86_64_SPECIAL:
						for (index = 0; index < 2; index++) {
							opctable_dp(v, &x86_64_table[dp->op[1].bytemode][index]);
						}
						break;
					default:
						ASSERT(0);
						break;
				}
			}
		} else {
			opctable_dp(v, dp);
		}
	}
}

	static void
opctable_fill_float_mem(btmod_vcpu *v, char const **fm, int *fm_dpnum, int fmsize)
{
	int i;
	for (i = 0; i < fmsize; i++) {
		opctable_float_mem(v, fm[i], &fm_dpnum[i]);
	}
}

	void
opc_init(btmod_vcpu *v)
{
	/* iterate over all dis386, dis386_twobyte; get the template string
	 * and add it to opctable.
	 */
	opctable_init(v->bt);
	opctable_fill(v, &dis386[0], sizeof dis386/sizeof dis386[0]);
	opctable_fill(v, &dis386_twobyte[0],
			sizeof dis386_twobyte/sizeof dis386_twobyte[0]);
	opctable_fill_float_mem(v, float_mem, float_mem_dpnum,
			sizeof float_mem_mode/sizeof float_mem_mode[0]);
	//insn_init_constants();
}


	static void
DIS_ST (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	//	oappend (bti386, "%st" + bti386->intel_syntax);
	op->type = op_st;
	op->val.st = 0;
	op->tag.st = tag_const;
}

	static void
DIS_STi (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	//	snprintf (bti386->scratchbuf, sizeof bti386->scratchbuf, "%%st(%d)", modrm.rm);
	//	oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
	op->type = op_st;
	op->val.st = bti386->modrm.rm;
	op->tag.st = tag_const;
}


	static int
get_seg (bti386_t *bti386)
{
	int ret = -1;
	if (bti386->prefixes & PREFIX_CS)
	{
		bti386->used_prefixes |= PREFIX_CS;
		//oappend (bti386, "%cs:" + bti386->intel_syntax);
		ret = cs_reg;
	}
	if (bti386->prefixes & PREFIX_DS)
	{
		bti386->used_prefixes |= PREFIX_DS;
		//oappend (bti386, "%ds:" + bti386->intel_syntax);
		ret = ds_reg;
	}
	if (bti386->prefixes & PREFIX_SS)
	{
		bti386->used_prefixes |= PREFIX_SS;
		//oappend (bti386, "%ss:" + bti386->intel_syntax);
		ret = ss_reg;
	}
	if (bti386->prefixes & PREFIX_ES)
	{
		bti386->used_prefixes |= PREFIX_ES;
		//oappend (bti386, "%es:" + bti386->intel_syntax);
		ret = es_reg;
	}
	if (bti386->prefixes & PREFIX_FS)
	{
		bti386->used_prefixes |= PREFIX_FS;
		//oappend (bti386, "%fs:" + bti386->intel_syntax);
		ret = fs_reg;
	}
	if (bti386->prefixes & PREFIX_GS)
	{
		bti386->used_prefixes |= PREFIX_GS;
		//oappend (bti386, "%gs:" + bti386->intel_syntax);
		ret = gs_reg;
	}
	if (ret == -1) {
		return -1;
	}

	ASSERT(ret - es_reg >= 0 && ret - es_reg < 6);
	return ret - es_reg;
}


	static void
DIS_indirE (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	/*
		 if (!bti386->intel_syntax)
		 oappend (bti386, "*");
		 */
	DIS_E (bti386, op, bytemode, sizeflag);
	/*
		 if (op->type == op_mem) {
		 ASSERT(op->val.mem.segtype == segtype_sel);
		 if (op->val.mem.seg.sel == -1) {
		 op->val.mem.seg.sel = R_CS;
		 }
		 }
		 */
}

#if 0
	static void
disas_operand_value (bti386_t *bti386, operand_t *op, char *buf, int hex, bfd_vma disp)
{
	ASSERT(0);
#if 0
	if (address_mode == mode_64bit)
	{
		if (hex)
		{
			char tmp[30];
			int i;
			buf[0] = '0';
			buf[1] = 'x';
			sprintf (tmp, "%x", disp);
			for (i = 0; tmp[i] == '0' && tmp[i + 1]; i++);
			strcpy (buf + 2, tmp + i);
		}
		else
		{
			bfd_signed_vma v = disp;
			char tmp[30];
			int i;
			if (v < 0)
			{
				*(buf++) = '-';
				v = -disp;
				/* Check for possible overflow on 0x8000000000000000.  */
				if (v < 0)
				{
					strcpy (buf, "9223372036854775808");
					return;
				}
			}
			if (!v)
			{
				strcpy (buf, "0");
				return;
			}

			i = 0;
			tmp[29] = 0;
			while (v)
			{
				tmp[28 - i] = (v % 10) + '0';
				v /= 10;
				i++;
			}
			strcpy (buf, tmp + 29 - i);
		}
	}
	else
	{
		if (hex)
			sprintf (buf, "0x%x", (unsigned int) disp);
		else
			sprintf (buf, "%d", (int) disp);
	}
#endif
}
#endif

/* Put DISP in BUF as signed hex number.  */

#if 0
	static void
disas_displacement (operand_t *op, char *buf, bfd_vma disp)
{
	ASSERT(0);
#if 0
	bfd_signed_vma val = disp;
	char tmp[30];
	int i, j = 0;

	if (val < 0)
	{
		buf[j++] = '-';
		val = -disp;

		/* Check for possible overflow.  */
		if (val < 0)
		{
			switch (address_mode)
			{
				case mode_64bit:
					strcpy (buf + j, "0x8000000000000000");
					break;
				case mode_32bit:
					strcpy (buf + j, "0x80000000");
					break;
				case mode_16bit:
					strcpy (buf + j, "0x8000");
					break;
			}
			return;
		}
	}

	buf[j++] = '0';
	buf[j++] = 'x';

	sprintf(tmp, "%x", val);
	for (i = 0; tmp[i] == '0'; i++)
		continue;
	if (tmp[i] == '\0')
		i--;
	strcpy (buf + j, tmp + i);
#endif
}
#endif

#if 0
	static void
disas_intel_operand_size (operand_t *op, int bytemode, int sizeflag)
{
	ASSERT(0);
#if 0
	switch (bytemode)
	{
		case b_mode:
		case dqb_mode:
			oappend (bti386, "BYTE PTR ");
			break;
		case w_mode:
		case dqw_mode:
			oappend (bti386, "WORD PTR ");
			break;
		case stack_v_mode:
			if (address_mode == mode_64bit && (sizeflag & DFLAG))
			{
				oappend (bti386, "QWORD PTR ");
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			}
			/* FALLTHRU */
		case v_mode:
		case dq_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oappend (bti386, "QWORD PTR ");
			else if ((sizeflag & DFLAG) || bytemode == dq_mode)
				oappend (bti386, "DWORD PTR ");
			else
				oappend (bti386, "WORD PTR ");
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case z_mode:
			if ((bti386->rex & REX_W) || (sizeflag & DFLAG))
				*(bti386->obufp)++ = 'D';
			oappend (bti386, "WORD PTR ");
			if (!(bti386->rex & REX_W))
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case d_mode:
		case dqd_mode:
			oappend (bti386, "DWORD PTR ");
			break;
		case q_mode:
			oappend (bti386, "QWORD PTR ");
			break;
		case m_mode:
			if (address_mode == mode_64bit)
				oappend (bti386, "QWORD PTR ");
			else
				oappend (bti386, "DWORD PTR ");
			break;
		case f_mode:
			if (sizeflag & DFLAG)
				oappend (bti386, "FWORD PTR ");
			else
				oappend (bti386, "DWORD PTR ");
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case t_mode:
			oappend (bti386, "TBYTE PTR ");
			break;
		case x_mode:
			oappend (bti386, "XMMWORD PTR ");
			break;
		case o_mode:
			oappend (bti386, "OWORD PTR ");
			break;
		default:
			break;
	}
#endif
}
#endif

	static int
get_operand_size (bti386_t *bti386, int bytemode, int sizeflag)
{
	switch (bytemode)
	{
		case b_mode:
		case dqb_mode:
			return 1;
			break;
		case w_mode:
		case dqw_mode:
			return 2;
			break;
		case stack_v_mode:
			if (address_mode == mode_64bit && (sizeflag & DFLAG))
			{
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				return 8;
				break;
			}
			/* FALLTHRU */
		case v_mode:
		case dq_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W) {
				return 8;
			} else if ((sizeflag & DFLAG) || bytemode == dq_mode) {
				return 4;
			} else {
				return 2;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case z_mode:
			if ((bti386->rex & REX_W) || (sizeflag & DFLAG)) {
				return 4;
				//*(bti386->obufp)++ = 'D';
			} else {
				return 2;
			}
			//oappend (bti386, "WORD PTR ");
			if (!(bti386->rex & REX_W)) {
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			}
			break;
		case d_mode:
		case dqd_mode:
			return 4;
			//oappend (bti386, "DWORD PTR ");
			break;
		case q_mode:
			return 8;
			//oappend (bti386, "QWORD PTR ");
			break;
		case m_mode:
			if (address_mode == mode_64bit) {
				return 8;
				//oappend (bti386, "QWORD PTR ");
			} else {
				return 4;
				//oappend (bti386, "DWORD PTR ");
			}
			break;
		case f_mode:
			if (sizeflag & DFLAG) {
				//oappend (bti386, "FWORD PTR ");
			} else {
				//oappend (bti386, "DWORD PTR ");
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case t_mode:
			//oappend (bti386, "TBYTE PTR ");
			break;
		case x_mode:
			//oappend (bti386, "XMMWORD PTR ");
			break;
		case o_mode:
			//oappend (bti386, "OWORD PTR ");
			break;
		default:
			break;
	}
	NOT_REACHED();
	return 0;
}


	static void
DIS_E (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_vma disp;
	int add = 0;
	int riprel = 0;
	USED_REX (REX_B);
	if (bti386->rex & REX_B) {
		add += 8;
	}

	op->tag.all = tag_const;
	op->type = invalid;

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;

	if (bti386->modrm.mod == 3)
	{
		op->type = op_reg;
		op->val.reg = bti386->modrm.rm + add;
		switch (bytemode)
		{
			case b_mode:
				USED_REX (0);
				op->size = 1;
				break;
			case w_mode:
				op->size = 2;
				break;
			case d_mode:
				op->size = 4;
				break;
			case q_mode:
				op->size = 8;
				break;
			case m_mode:
				if (address_mode == mode_64bit) {
					op->size = 8;
				} else {
					op->size = 4;
				}
				break;
			case stack_v_mode:
				if (address_mode == mode_64bit && (sizeflag & DFLAG))
				{
					op->size = 8;
					bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
					break;
				}
				bytemode = v_mode;
				/* FALLTHRU */
			case v_mode:
			case dq_mode:
			case dqb_mode:
			case dqd_mode:
			case dqw_mode:
				USED_REX (REX_W);
				if (bti386->rex & REX_W) {
					op->size = 8;
				} else if ((sizeflag & DFLAG) || bytemode != v_mode) {
					op->size = 4;
				} else {
					op->size = 2;
				}
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
				break;
			case 0:
				op->type = invalid;
				break;
			default:
				//ASSERT(0);
				BadOp(bti386);
				op->type = invalid;
				break;
		}
		return;
	}

	op->type = op_mem;
	disp = 0;
	op->val.mem.segtype = segtype_sel;
	op->val.mem.seg.sel = get_seg (bti386);

	if ((sizeflag & AFLAG) || address_mode == mode_64bit) {
		/* 32/64 bit address mode */
		int havedisp;
		int havesib;
		int havebase;
		int base;
		int index = 0;
		int scale = 0;

		havesib = 0;
		havebase = 1;
		base = bti386->modrm.rm;

		if (base == 4) {
			havesib = 1;
			index = (dis_ldub(bti386->codep) >> 3) & 7;
			if (address_mode == mode_64bit || index != 0x4) {
				/* When INDEX == 0x4 in 32 bit mode, SCALE is ignored.  */
				scale = (dis_ldub(bti386->codep) >> 6) & 3;
			}
			base = dis_ldub(bti386->codep) & 7;
			USED_REX (REX_X);
			if (bti386->rex & REX_X) {
				index += 8;
			}
			bti386->codep++;
		}
		base += add;

		ASSERT(base >= 0 && base < NUM_REGS);

		switch (bti386->modrm.mod) {
			case 0:
				if ((base & 7) == 5) {
					havebase = 0;
					if (address_mode == mode_64bit && !havesib)
						riprel = 1;
					disp = get32s (bti386);
				}
				break;
			case 1:
				disp = dis_ldub(bti386->codep++);
				if ((disp & 0x80) != 0)
					disp -= 0x100;
				break;
			case 2:
				disp = get32s (bti386);
				break;
		}

		havedisp = havebase || (havesib && (index != 4 || scale != 0));

		if (bti386->modrm.mod != 0 || (base & 7) == 5) {
			op->val.mem.disp = disp;
			if (riprel) {
				/*
					 set_op (disp, 1);
					 oappend (bti386, "(%rip)");
					 */
			}
		}

		if  (address_mode == mode_64bit && (sizeflag & AFLAG)) {
			op->size = 8;
			op->val.mem.addrsize = 8;
		} else {
			op->size = 4;
			op->val.mem.addrsize = 4;
		}

		if (havedisp) {
			//*(bti386->obufp)++ = bti386->open_char;
			//*(bti386->obufp) = '\0';
			if (havebase) {
				//oappend (bti386, address_mode == mode_64bit && (sizeflag & AFLAG)
				//? names64[base] : names32[base]);
				op->val.mem.base = base;
				//ADD_MEM_BASE(op->val, base);
				ASSERT(op->val.mem.base >= 0 && op->val.mem.base < NUM_REGS);
			}
			if (havesib) {
				if (scale != 0 || index != 4) {
					op->val.mem.scale = 1 << scale;
					//ADD_MEM_SCALE(op->val, 1 << scale);
				}
				if (index != 4)
				{
					op->val.mem.index = index;
					//ADD_MEM_INDEX(op->val, index);
					/*
						 oappend (bti386, address_mode == mode_64bit && (sizeflag & AFLAG)
						 ? names64[index] : names32[index]);
						 */
				}
			}
		}

		if (op->val.mem.seg.sel == -1) {
			op->val.mem.seg.sel = R_DS;
			//op->val.mem.seg.sel = default_seg(op->val.mem.base);
		}
	} else { /* 16 bit address mode */
		op->size = 2;
		op->val.mem.addrsize = 2;
		switch (bti386->modrm.mod) {
			case 0:
				if (bti386->modrm.rm == 6) {
					disp = get16 (bti386);
					if ((disp & 0x8000) != 0)
						disp -= 0x10000;
				}
				break;
			case 1:
				disp = dis_ldub(bti386->codep++);
				if ((disp & 0x80) != 0)
					disp -= 0x100;
				break;
			case 2:
				disp = get16 (bti386);
				if ((disp & 0x8000) != 0)
					disp -= 0x10000;
				break;
		}

		if (bti386->modrm.mod != 0 || bti386->modrm.rm == 6) {
			op->val.mem.disp = disp;
			/*
				 print_displacement (bti386->scratchbuf, disp);
				 oappend (bti386, bti386->scratchbuf);
				 */
		}

		if (bti386->modrm.mod != 0 || bti386->modrm.rm != 6) {
			if (bti386->modrm.rm < 2) {
				op->val.mem.base = eBX_reg - eAX_reg;
				op->val.mem.index = (eSI_reg - eAX_reg) + (bti386->modrm.rm % 2);
				op->val.mem.scale = 1;
			} else if (bti386->modrm.rm < 4) {
				op->val.mem.base = eBP_reg - eAX_reg;
				op->val.mem.index = (eSI_reg - eAX_reg) + (bti386->modrm.rm % 2);
				op->val.mem.scale = 1;
			} else if (bti386->modrm.rm < 6) {
				op->val.mem.base = (eSI_reg - eAX_reg) + (bti386->modrm.rm % 2);
				op->val.mem.index = -1;
				op->val.mem.scale = 0;
			} else if (bti386->modrm.rm == 6) {
				op->val.mem.base = (eBP_reg - eAX_reg);
				op->val.mem.index = -1;
				op->val.mem.scale = 0;
			} else if (bti386->modrm.rm == 7) {
				op->val.mem.base = (eBX_reg - eAX_reg);
				op->val.mem.index = -1;
				op->val.mem.scale = 0;
			}
			ASSERT(op->val.mem.base == -1
					|| (op->val.mem.base >= 0 && op->val.mem.base < NUM_REGS));
			ASSERT(op->val.mem.index == -1
					|| (op->val.mem.index >= 0 && op->val.mem.index < NUM_REGS));
			//op->val = modrm.rm;
			//*(bti386->obufp)++ = bti386->open_char;
			//*(bti386->obufp) = '\0';
			//oappend (bti386, index16[modrm.rm]);

			//*(bti386->obufp)++ = bti386->close_char;
			//*(bti386->obufp) = '\0';
		}
		ASSERT(op->val.mem.base == -1
				|| (op->val.mem.base >= 0 && op->val.mem.base < NUM_REGS));
		ASSERT(op->val.mem.index == -1
				|| (op->val.mem.index >= 0 && op->val.mem.index < NUM_REGS));
		if (op->val.mem.seg.sel == -1) {
			//op->val.mem.seg.sel = default_seg(op->val.mem.base);
			if (op->val.mem.base  == R_EBP) {
				op->val.mem.seg.sel = R_SS;
			} else {
				op->val.mem.seg.sel = R_DS;
			}
		}
	}
	ASSERT(op->type != invalid);
}

	static void
DIS_G (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R) {
		add += 8;
	}

	op->type = op_reg;
	op->val.reg = bti386->modrm.reg + add;

	switch (bytemode)
	{
		case b_mode:
			USED_REX (0);
			op->size = 1;
			if (bti386->rex) {
				//oappend (bti386, names8rex[modrm.reg + add]);
			} else {
				//oappend (bti386, names8[modrm.reg + add]);
			}
			break;
		case w_mode:
			op->size = 2;
			//oappend (bti386, names16[modrm.reg + add]);
			break;
		case d_mode:
			op->size = 4;
			//oappend (bti386, names32[modrm.reg + add]);
			break;
		case q_mode:
			op->size = 8;
			//oappend (bti386, names64[modrm.reg + add]);
			break;
		case v_mode:
		case dq_mode:
		case dqb_mode:
		case dqd_mode:
		case dqw_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W) {
				op->size = 8;
				//oappend (bti386, names64[modrm.reg + add]);
			} else if ((sizeflag & DFLAG) || bytemode != v_mode) {
				op->size = 4;
				//oappend (bti386, names32[modrm.reg + add]);
			} else {
				op->size = 2;
				//oappend (bti386, names16[modrm.reg + add]);
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case m_mode:
			if (address_mode == mode_64bit) {
				op->size = 8;
				//oappend (bti386, names64[modrm.reg + add]);
			} else {
				op->size = 4;
				//oappend (bti386, names32[modrm.reg + add]);
			}
			break;
		default:
			ASSERT(0);
			break;
	}
}

	static void
DIS_REG (bti386_t *bti386, operand_t *op, int code, int sizeflag)
{
	//const char *s;
	int add = 0;
	USED_REX (REX_B);
	if (bti386->rex & REX_B) {
		add = 8;
	}

	switch (code)
	{
		case ax_reg: case cx_reg: case dx_reg: case bx_reg:
		case sp_reg: case bp_reg: case si_reg: case di_reg:
			op->type = op_reg;
			op->size = 2;
			op->val.reg = code - ax_reg + add;
			//s = names16[code - ax_reg + add];
			break;
		case es_reg: case ss_reg: case cs_reg:
		case ds_reg: case fs_reg: case gs_reg:
			op->type = op_seg;
			op->size = 0;
			op->val.seg = (code - es_reg + add);
			ASSERT(op->val.seg >= 0 && op->val.seg < 6);
			//s = names_seg[code - es_reg + add];
			break;
		case al_reg: case ah_reg: case cl_reg: case ch_reg:
		case dl_reg: case dh_reg: case bl_reg: case bh_reg:
			USED_REX (0);
			op->size = 1;
			op->type = op_reg;
			if (bti386->rex) {
				op->rex_used = 1;
				op->val.reg = code - al_reg + add;
				//s = names8rex[code - al_reg + add];
			} else {
				op->val.reg = code - al_reg;
				//s = names8[code - al_reg];
			}
			break;
		case rAX_reg: case rCX_reg: case rDX_reg: case rBX_reg:
		case rSP_reg: case rBP_reg: case rSI_reg: case rDI_reg:
			if (address_mode == mode_64bit && (sizeflag & DFLAG)) {
				op->size = 8;
				op->type = op_reg;
				op->val.reg = code - rAX_reg + add;
				//s = names64[code - rAX_reg + add];
				break;
			}
			code += eAX_reg - rAX_reg;
			/* Fall through.  */
		case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
		case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
			USED_REX (REX_W);
			op->type = op_reg;
			if (bti386->rex & REX_W) {
				op->rex_used = 1;
				op->size = 8;
				op->val.reg = code - eAX_reg + add;
				//s = names64[code - eAX_reg + add];
			} else if (sizeflag & DFLAG) {
				op->size = 4;
				op->val.reg = code - eAX_reg + add;
				//s = names32[code - eAX_reg + add];
			} else {
				op->size = 2;
				op->val.reg = code - eAX_reg + add;
				//s = names16[code - eAX_reg + add];
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			ASSERT(0);
			//s = INTERNAL_DISASSEMBLER_ERROR;
			break;
	}
	//oappend (bti386, s);
}

	static void
DIS_IMREG (bti386_t *bti386, operand_t *op, int code, int sizeflag)
{
	const char *s;

	switch (code)
	{
		case indir_dx_reg:
			op->size = 2;
			op->type = op_reg;
			op->val.reg = R_EDX;
			op->tag.reg = tag_const;
			//s = "(%dx)";
			break;
		case ax_reg: case cx_reg: case dx_reg: case bx_reg:
		case sp_reg: case bp_reg: case si_reg: case di_reg:
			op->size = 2;
			op->type = op_reg;
			op->val.reg = code - ax_reg;
			op->tag.reg = tag_const;
			//s = names16[code - ax_reg];
			break;
		case es_reg: case ss_reg: case cs_reg:
		case ds_reg: case fs_reg: case gs_reg:
			op->size = 0;
			op->type = op_seg;
			op->val.seg = code - es_reg;
			op->tag.reg = tag_const;
			ASSERT(op->val.seg >= 0 && op->val.seg < 6);
			//s = names_seg[code - es_reg];
			break;
		case al_reg: case ah_reg: case cl_reg: case ch_reg:
		case dl_reg: case dh_reg: case bl_reg: case bh_reg:
			USED_REX (0);
			if (bti386->rex) {
				op->rex_used = 1;
				op->size = 1;
				op->type = op_reg;
				op->val.reg = code - al_reg;
				op->tag.reg = tag_const;
				//s = names8rex[code - al_reg];
			} else {
				op->size = 1;
				op->type = op_reg;
				op->val.reg = code - al_reg;
				op->tag.reg = tag_const;
				//s = names8[code - al_reg];
			}
			break;
		case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
		case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
			USED_REX (REX_W);
			if (bti386->rex & REX_W) {
				op->size = 8;
				op->type = op_reg;
				op->val.reg = code - eAX_reg;
				op->tag.reg = tag_const;
				//s = names64[code - eAX_reg];
			} else if (sizeflag & DFLAG) {
				op->size = 4;
				op->type = op_reg;
				op->val.reg = code - eAX_reg;
				op->tag.reg = tag_const;
				//s = names32[code - eAX_reg];
			} else {
				op->size = 2;
				op->type = op_reg;
				op->val.reg = code - eAX_reg;
				op->tag.reg = tag_const;
				//s = names16[code - eAX_reg];
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case z_mode_ax_reg:
			if ((bti386->rex & REX_W) || (sizeflag & DFLAG)) {
				op->size = 4;
				op->type = op_reg;
				op->val.reg = 0;
				op->tag.reg = tag_const;
				//s = *names32;
			} else {
				op->size = 2;
				op->type = op_reg;
				op->val.reg = 0;
				op->tag.reg = tag_const;
				//s = *names16;
			}
			if (!(bti386->rex & REX_W)) {
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			}
			break;
		default:
			s = INTERNAL_DISASSEMBLER_ERROR;
			break;
	}
	//oappend (bti386, s);
}

	static void
DIS_I (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_signed_vma oper;
	bfd_signed_vma mask = -1;
	op->type = op_imm;
	op->size = 0;

	switch (bytemode)
	{
		case b_mode:
			oper = dis_ldub(bti386->codep++);
			mask = 0xff;
			break;
		case q_mode:
			if (address_mode == mode_64bit)
			{
				oper = get32s (bti386);
				break;
			}
			/* Fall through.  */
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oper = get32s (bti386);
			else if (sizeflag & DFLAG)
			{
				oper = get32 (bti386);
				mask = 0xffffffff;
			}
			else
			{
				oper = get16 (bti386);
				mask = 0xfffff;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			mask = 0xfffff;
			oper = get16 (bti386);
			break;
		case const_1_mode:
			/*if (bti386->intel_syntax)
				oappend (bti386, "1");
				return;*/
			oper = 1;
			break;
		default:
			ASSERT(0);
			//oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			return;
	}

	oper &= mask;
	op->val.imm = oper;
}

	static void
DIS_I64 (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_signed_vma oper;
	bfd_signed_vma mask = -1;

	//op->type = op_imm;
	//op->size = 0;

	if (address_mode != mode_64bit) {
		//OP_I (bti386, bytemode, sizeflag);
		DIS_I (bti386, op, bytemode, sizeflag);
		return;
	}
	op->type = op_imm;
	op->size = 0;

	switch (bytemode) {
		case b_mode:
			oper = dis_ldub(bti386->codep++);
			mask = 0xff;
			break;
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oper = get64 (bti386);
			else if (sizeflag & DFLAG) {
				oper = get32 (bti386);
				mask = 0xffffffff;
			}
			else
			{
				oper = get16 (bti386);
				mask = 0xfffff;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			mask = 0xfffff;
			oper = get16 (bti386);
			break;
		default:
			//oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			//return;
			oper = 0;
	}

	oper &= mask;

	op->val.imm = oper;
}

	static void
DIS_sI (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_signed_vma oper;
	bfd_signed_vma mask = -1;

	op->type = op_imm;
	op->size = 0;

	switch (bytemode)
	{
		case b_mode:
			oper = dis_ldub(bti386->codep++);
			if ((oper & 0x80) != 0)
				oper -= 0x100;
			mask = 0xffffffff;
			break;
		case v_mode:
			USED_REX (REX_W);
			if (bti386->rex & REX_W)
				oper = get32s (bti386);
			else if (sizeflag & DFLAG)
			{
				oper = get32s (bti386);
				mask = 0xffffffff;
			}
			else
			{
				mask = 0xffffffff;
				oper = get16 (bti386);
				if ((oper & 0x8000) != 0)
					oper -= 0x10000;
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		case w_mode:
			oper = get16 (bti386);
			mask = 0xffffffff;
			if ((oper & 0x8000) != 0)
				oper -= 0x10000;
			break;
		default:
			//oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			//return;
			oper = 0;
	}

	op->val.imm = oper;
}

	static void
DIS_J (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_vma disp;
	bfd_vma mask = -1;
	bfd_vma segment = 0;

	switch (bytemode)
	{
		case b_mode:
			disp = dis_ldub(bti386->codep++);
			if ((disp & 0x80) != 0)
				disp -= 0x100;
			break;
		case v_mode:
			if ((sizeflag & DFLAG) || (bti386->rex & REX_W))
				disp = get32s (bti386);
			else
			{
				disp = get16 (bti386);
				if ((disp & 0x8000) != 0)
					disp -= 0x10000;
				/* In 16bit mode, address is wrapped around at 64k within
					 the same segment.  Otherwise, a data16 prefix on a jump
					 instruction means that the pc is masked to 16 bits after
					 the displacement is added!  */
				mask = 0xffff;
				/*
					 if ((bti386->prefixes & PREFIX_DATA) == 0)
					 segment = ((bti386->start_pc + bti386->codep - bti386->start_codep)
					 & ~((bfd_vma) 0xffff));
					 */
			}
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			break;
		default:
			//oappend (bti386, INTERNAL_DISASSEMBLER_ERROR);
			//return;
			disp = 0;
			break;
	}
	op->type = op_imm;
	//op->type = op_pcrel;
	op->size = 0;
	op->val.imm = ((bti386->codep - bti386->start_codep + disp) & mask) | segment;
	//op->val.pcrel = disp;
	/*printf("%s(): op->val.imm = %llx, segment=%llx, mask=%llx, bti386->codep=%p, bti386->start_codep=%p, disp=%llx\n", __func__, op->val.imm,
		(uint64_t)segment, (uint64_t)mask, bti386->codep, bti386->start_codep, disp);*/
}

	static void
DIS_SEG (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	op->type = op_seg;
	if (bytemode == w_mode) {
		op->val.seg = bti386->modrm.reg;
		//ASSERT(op->val.seg >= 0 && op->val.seg < 6);
		if (!(op->val.seg >= 0 && op->val.seg < 6)) {
			op->type = invalid;
		}
		//oappend (bti386, names_seg[modrm.reg]);
	} else {
		DIS_E (bti386, op, bti386->modrm.mod == 3 ? bytemode : w_mode, sizeflag);
	}
}

	static void
DIS_DIR (bti386_t *bti386, operand_t *op, int dummy ATTRIBUTE_UNUSED, int sizeflag)
{
	int seg, offset;

	if (sizeflag & DFLAG)
	{
		offset = get32 (bti386);
		seg = get16 (bti386);
	}
	else
	{
		offset = get16 (bti386);
		seg = get16 (bti386);
	}
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);

	op->type = op_mem;
	//INIT_MEM_VAL(op->val);
	op->val.mem.segtype = segtype_desc;
	op->val.mem.seg.desc = seg;
	//ADD_MEM_SEGDESC(op->val, seg);
	op->val.mem.disp = offset;
	op->size = 4;     /* use a default size (similar to DIS_M). */
}

	static void
DIS_OFF (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_vma off;

	/*
		 if (bti386->intel_syntax && (sizeflag & SUFFIX_ALWAYS)) {
		 intel_operand_size (bytemode, sizeflag);
		 }
		 */
	//INIT_MEM_VAL(op->val);
	//ADD_MEM_SEG(op->val, get_seg (bti386));
	//append_seg (bti386);

	if ((sizeflag & AFLAG) || address_mode == mode_64bit) {
		off = get32 (bti386);
	} else {
		off = get16 (bti386);
	}

	op->type = op_mem;
	op->val.mem.disp = off;
	op->val.mem.segtype = segtype_sel;
	op->val.mem.seg.sel = get_seg (bti386);
	if (op->val.mem.seg.sel == -1) {
		op->val.mem.seg.sel = R_DS;
	}
	op->size = get_operand_size (bti386, bytemode, sizeflag);
}

	static void
DIS_OFF64 (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bfd_vma off;

	if (address_mode != mode_64bit || (bti386->prefixes & PREFIX_ADDR)) {
		DIS_OFF (bti386, op, bytemode, sizeflag);
		return;
	}

	//INIT_MEM_VAL(op->val);
	op->size = get_operand_size (bti386, bytemode, sizeflag);
	//ADD_MEM_SEG(op->val, get_seg (bti386));
	op->val.mem.segtype = segtype_sel;
	op->val.mem.seg.sel = get_seg (bti386);
	//append_seg (bti386);

	off = get64 (bti386);

	/*
		 if (bti386->intel_syntax)
		 {
		 if (!(bti386->prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
		 | PREFIX_ES | PREFIX_FS | PREFIX_GS)))
		 {
		 oappend (bti386, names_seg[ds_reg - es_reg]);
		 oappend (bti386, ":");
		 }
		 }
		 */
	op->val.mem.disp = off;
}

	static void
DIS_ESreg (bti386_t *bti386, operand_t *op, int code, int sizeflag)
{
	op->type = op_mem;
	switch (dis_ldub(bti386->codep -1))
	{
		case 0x6d:	/* insw/insl */
			op->size = get_operand_size (bti386, z_mode, sizeflag);
			//intel_operand_size (z_mode, sizeflag);
			break;
		case 0xa5:	/* movsw/movsl/movsq */
		case 0xa7:	/* cmpsw/cmpsl/cmpsq */
		case 0xab:	/* stosw/stosl */
		case 0xaf:	/* scasw/scasl */
			op->size = get_operand_size (bti386, v_mode, sizeflag);
			//intel_operand_size (v_mode, sizeflag);
			break;
		default:
			op->size = get_operand_size (bti386, b_mode, sizeflag);
			//intel_operand_size (b_mode, sizeflag);
	}
	//INIT_MEM_VAL(op->val);
	op->val.mem.base = code - eAX_reg;
	op->val.mem.seg.sel = es_reg - es_reg;
	op->val.mem.segtype = segtype_sel;

	ASSERT(op->val.mem.base >= 0 && op->val.mem.base < NUM_REGS);
	/*
		 oappend (bti386, "%es:" + bti386->intel_syntax);
		 ptr_reg (bti386, code, sizeflag);
		 */
}

	static void
DIS_DSreg (bti386_t *bti386, operand_t *op, int code, int sizeflag)
{
	op->type = op_mem;
	switch (dis_ldub(bti386->codep -1))
	{
		case 0x6f:	/* outsw/outsl */
			op->size = get_operand_size (bti386, z_mode, sizeflag);
			//intel_operand_size (z_mode, sizeflag);
			break;
		case 0xa5:	/* movsw/movsl/movsq */
		case 0xa7:	/* cmpsw/cmpsl/cmpsq */
		case 0xad:	/* lodsw/lodsl/lodsq */
			op->size = get_operand_size (bti386, v_mode, sizeflag);
			//intel_operand_size (v_mode, sizeflag);
			break;
		default:
			op->size = get_operand_size (bti386, b_mode, sizeflag);
			//intel_operand_size (b_mode, sizeflag);
	}
	if ((bti386->prefixes
				& ( PREFIX_CS
					| PREFIX_DS
					| PREFIX_SS
					| PREFIX_ES
					| PREFIX_FS
					| PREFIX_GS)) == 0) {
		bti386->prefixes |= PREFIX_DS;
	}
	//INIT_MEM_VAL(op->val);
	op->val.mem.base = code - eAX_reg;
	op->val.mem.seg.sel = get_seg (bti386);
	op->val.mem.segtype = segtype_sel;

	if (address_mode == mode_64bit) {
		if (!(sizeflag & AFLAG)) {
			op->val.mem.addrsize = 4;
		} else {
			op->val.mem.addrsize = 8;
		}
	} else if (sizeflag & AFLAG) {
		op->val.mem.addrsize = 4;
	} else {
		op->val.mem.addrsize = 2;
	}
	ASSERT(op->val.mem.base >= 0 && op->val.mem.base < NUM_REGS);
	//append_seg (bti386);
	/*
		 ptr_reg (bti386, code, sizeflag);
		 */
}

	static void
DIS_C (bti386_t *bti386, operand_t *op, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	if (bti386->rex & REX_R)
	{
		USED_REX (REX_R);
		add = 8;
	}
	else if (address_mode != mode_64bit && (bti386->prefixes & PREFIX_LOCK))
	{
		bti386->used_prefixes |= PREFIX_LOCK;
		add = 8;
	}
	op->type = op_cr;
	op->val.cr = bti386->modrm.reg + add;
}

	static void
DIS_D (bti386_t *bti386, operand_t *op, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R) {
		add = 8;
	}
	op->type = op_db;
	op->val.db = bti386->modrm.reg + add;
}

	static void
DIS_T (bti386_t *bti386, operand_t *op, int dummy ATTRIBUTE_UNUSED, int sizeflag ATTRIBUTE_UNUSED)
{
	op->type = op_tr;
	op->val.tr = bti386->modrm.reg;
}

	static void
DIS_R (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3) {
		DIS_E (bti386, op, bytemode, sizeflag);
	} else {
		BadOp (bti386);
	}
}

	static void
DIS_MMX (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	if (bti386->prefixes & PREFIX_DATA)
	{
		int add = 0;
		USED_REX (REX_R);
		if (bti386->rex & REX_R) {
			add = 8;
		}
		op->type = op_xmm;
		op->val.xmm = bti386->modrm.reg + add;
	} else {
		op->type = op_mmx;
		op->val.mmx = bti386->modrm.reg;
	}
	//oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

	static void
DIS_XMM (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	int add = 0;
	USED_REX (REX_R);
	if (bti386->rex & REX_R) {
		add = 8;
	}
	op->type = op_xmm;
	op->val.xmm = bti386->modrm.reg + add;
}

	static void
DIS_EM (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod != 3)
	{
		if (bytemode == v_mode)
		{
			bytemode = (bti386->prefixes & PREFIX_DATA) ? x_mode : q_mode;
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
		}
		DIS_E (bti386, op, bytemode, sizeflag);
		return;
	}

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	if (bti386->prefixes & PREFIX_DATA) {
		int add = 0;

		USED_REX (REX_B);
		if (bti386->rex & REX_B) {
			add = 8;
		}
		op->type = op_xmm;
		op->val.xmm = bti386->modrm.rm + add;
	}
	else {
		op->type = op_mmx;
		op->val.mmx = bti386->modrm.rm;
	}
	//oappend (bti386, bti386->scratchbuf + bti386->intel_syntax);
}

/* cvt* are the only instructions in sse2 which have
	 both SSE and MMX operands and also have 0x66 prefix
	 in their opcode. 0x66 was originally used to differentiate
	 between SSE and MMX instruction(operands). So we have to handle the
	 cvt* separately using OP_EMC and OP_MXC */
	static void
DIS_EMC (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod != 3)
	{
		if (bytemode == v_mode)
		{
			bytemode = (bti386->prefixes & PREFIX_DATA) ? x_mode : q_mode;
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
		}
		DIS_E (bti386, op, bytemode, sizeflag);
		return;
	}

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	op->type = op_mmx;
	op->val.mmx = bti386->modrm.rm;
}

	static void
DIS_MXC (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
	op->type = op_mmx;
	op->val.mmx = bti386->modrm.reg;
}

	static void
DIS_EX (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	int add = 0;
	if (bti386->modrm.mod != 3)
	{
		DIS_E (bti386, op, bytemode, sizeflag);
		return;
	}
	USED_REX (REX_B);
	if (bti386->rex & REX_B) {
		add = 8;
	}

	/* Skip mod/rm byte.  */
	MODRM_CHECK;
	bti386->codep++;
	op->type = op_xmm;
	op->val.xmm = bti386->modrm.rm + add;
}

	static void
DIS_MS (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3) {
		DIS_EM (bti386, op, bytemode, sizeflag);
	} else {
		BadOp (bti386);
	}
}

	static void
DIS_XS (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3) {
		DIS_EX (bti386, op, bytemode, sizeflag);
	} else {
		BadOp (bti386);
	}
}

	static void
DIS_M (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3) {
		/* bad bound,lea,lds,les,lfs,lgs,lss,cmpxchg8b,vmptrst modrm */
		BadOp (bti386);
	} else {
		DIS_E (bti386, op, bytemode, sizeflag);
		op->size = 4;     /* Use a default size. */
	}
}

	static void
DIS_SVME_Fixup(bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	const char *alt;
	//char *p;

	switch (dis_ldub(bti386->codep))
	{
		case 0xd8:
			alt = "vmrun";
			break;
		case 0xd9:
			alt = "vmmcall";
			break;
		case 0xda:
			alt = "vmload";
			break;
		case 0xdb:
			alt = "vmsave";
			break;
		case 0xdc:
			alt = "stgi";
			break;
		case 0xdd:
			alt = "clgi";
			break;
		case 0xde:
			alt = "skinit";
			break;
		case 0xdf:
			alt = "invlpga";
			break;
		default:
			DIS_M (bti386, op, bytemode, sizeflag);
			return;
	}
	ASSERT(0);
#if 0
	/* Override "lidt".  */
	p = obuf + strlen (obuf) - 4;
	/* We might have a suffix.  */
	if (*p == 'i')
		--p;
	strlcpy (p, alt, ((char *)obuf + sizeof obuf) - p);
	if (!(bti386->prefixes & PREFIX_ADDR))
	{
		++bti386->codep;
		return;
	}
	bti386->used_prefixes |= PREFIX_ADDR;
	switch (*bti386->codep++)
	{
		case 0xdf:
			strlcpy (bti386->op_out[1], names32[1], sizeof bti386->op_out[1]);
			two_source_ops = 1;
			/* Fall through.  */
		case 0xd8:
		case 0xda:
		case 0xdb:
			*(bti386->obufp)++ = bti386->open_char;
			if (address_mode == mode_64bit || (sizeflag & AFLAG))
				alt = names32[0];
			else
				alt = names16[0];
			strlcpy ((bti386->obufp), alt, ((char *)obuf + sizeof obuf) - (bti386->obufp));
			(bti386->obufp) += strlen (alt);
			*(bti386->obufp)++ = bti386->close_char;
			*(bti386->obufp) = '\0';
			break;
	}
#endif
}

static void
DIS_INVLPG_Fixup (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	const char *alt;

	switch (dis_ldub(bti386->codep))
	{
		case 0xf8:
			alt = "swapgs";
			break;
		case 0xf9:
			alt = "rdtscp";
			break;
		default:
			DIS_M (bti386, op, bytemode, sizeflag);
			return;
	}
	/* Override "invlpg".  */
  strlcpy (bti386->op_out[0], alt, sizeof bti386->op_out[0]);
	bti386->codep++;
}

static void
DIS_VMX_Fixup (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3
			&& bti386->modrm.reg == 0
			&& bti386->modrm.rm >=1
			&& bti386->modrm.rm <= 4)
	{
		/* Override "sgdt".  */
		//char *p = (bti386->obuf) + strlen ((bti386->obuf)) - 4;

		/* We might have a suffix when disassembling with -Msuffix.  */
		/*if (*p == 'g')
			--p;*/

		switch (bti386->modrm.rm)
		{
			case 1:
				//strlcpy (p, "vmcall", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				bti386->rename_opcode = "vmcall";
				break;
			case 2:
				//strlcpy (p, "vmlaunch", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				bti386->rename_opcode = "vmlaunch";
				break;
			case 3:
				//strlcpy (p, "vmresume", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				bti386->rename_opcode = "vmresume";
				break;
			case 4:
				//strlcpy (p, "vmxoff", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
				bti386->rename_opcode = "vmxoff";
				break;
		}

		bti386->codep++;
	}
	else
		DIS_E (bti386, op, bytemode, sizeflag);
}

static void
DIS_0fae (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if (bti386->modrm.mod == 3) {
		if (bti386->modrm.reg == 7) {
			bti386->rename_opcode = "sfence";
		}
		if (bti386->modrm.reg < 5 || bti386->modrm.rm != 0) {
		  ASSERT(0);
			BadOp (bti386);	// bad sfence, mfence, or lfence
			return;
		}
	} else if (bti386->modrm.reg != 7) {
		//ASSERT(0);
		//BadOp (bti386);		// bad clflush
		//return;
	}

	DIS_E (bti386, op, bytemode, sizeflag);
}

	static void
DIS_VMX (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	bti386->used_prefixes |= (bti386->prefixes & (PREFIX_DATA | PREFIX_REPZ));
	if (bti386->prefixes & PREFIX_DATA) {
	  bti386->rename_opcode = "vmclear";
	} else if (bti386->prefixes & PREFIX_REPZ) {
	  bti386->rename_opcode = "vmxon";
	} else {
	  bti386->rename_opcode = "vmptrld";
	}
	DIS_E (bti386, op, bytemode, sizeflag);
}

	static void
DIS_3DNowSuffix (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	const char *mnemonic;

	/* AMD 3DNow! instructions are specified by an opcode suffix in the
		 place where an 8-bit immediate would normally go.  ie. the last
		 byte of the instruction.  */
	//(bti386->obufp) = obuf + strlen (obuf);
	mnemonic = Suffix3DNow[dis_ldub(bti386->codep++) & 0xff];
	if (mnemonic) {
		op->type = op_3dnow;
		op->val.d3now = (dis_ldub(bti386->codep - 1)) & 0xff;
		//oappend (bti386, mnemonic);
	} else {
		BadOp (bti386);
	}
}

	static void
DIS_SIMD_Suffix (bti386_t *bti386, operand_t *op, int bytemode ATTRIBUTE_UNUSED,
		int sizeflag ATTRIBUTE_UNUSED)
{
	unsigned int cmp_type;

	cmp_type = dis_ldub(bti386->codep++) & 0xff;
	if (cmp_type < 8) {
		char suffix1 = 'p', suffix2 = 's';
		bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
		if (bti386->prefixes & PREFIX_REPZ) {
			suffix1 = 's';
		} else {
			bti386->used_prefixes |= (bti386->prefixes & PREFIX_DATA);
			if (bti386->prefixes & PREFIX_DATA) {
				suffix2 = 'd';
			} else {
				bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPNZ);
				if (bti386->prefixes & PREFIX_REPNZ) {
					suffix1 = 's', suffix2 = 'd';
				}
			}
		}
		bti386->used_prefixes |= (bti386->prefixes & PREFIX_REPZ);
	} else {
		BadOp (bti386);
	}
}

	static void
DIS_REP_Fixup (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	/* The 0xf3 prefix should be displayed as "rep" for ins, outs, movs,
		 lods and stos.  */
	size_t ilen = 0;

	if (bti386->prefixes & PREFIX_REPZ) {
		switch (dis_ldub(bti386->insn_codep))
		{
			case 0x6e:	/* outsb */
			case 0x6f:	/* outsw/outsl */
			case 0xa4:	/* movsb */
			case 0xa5:	/* movsw/movsl/movsq */
				ilen = 5;
				break;
			case 0xaa:	/* stosb */
			case 0xab:	/* stosw/stosl/stosq */
			case 0xac:	/* lodsb */
			case 0xad:	/* lodsw/lodsl/lodsq */
				if (sizeflag & SUFFIX_ALWAYS) {
					ilen = 5;
				}
				break;
			case 0x6c:	/* insb */
			case 0x6d:	/* insl/insw */
				ilen = 4;
				break;
			default:
				ABORT ();
				break;
		}
	}

#if 0
	if (ilen != 0) {
		size_t olen;
		char *p;

		olen = strlen (obuf);
		p = obuf + olen - ilen - 1 - 4;
		/* Handle "repz [addr16|addr32]".  */
		if ((bti386->prefixes & PREFIX_ADDR))
			p -= 1 + 6;

		memmove (p + 3, p + 4, olen - (p + 3 - obuf));
	}
#endif

	switch (bytemode)
	{
		case al_reg:
		case eAX_reg:
		case indir_dx_reg:
			DIS_IMREG (bti386, op, bytemode, sizeflag);
			break;
		case eDI_reg:
			DIS_ESreg (bti386, op, bytemode, sizeflag);
			break;
		case eSI_reg:
			DIS_DSreg (bti386, op, bytemode, sizeflag);
			break;
		default:
			ABORT ();
			break;
	}
}

	static void
DIS_Prefix (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	int p;
	p = bti386->prefixes;
	p &= ~(  PREFIX_CS | PREFIX_SS | PREFIX_DS | PREFIX_ES | PREFIX_FS
			| PREFIX_GS | PREFIX_DATA | PREFIX_ADDR);
	if (p != 0) {
		op->type = op_prefix;
		op->size = 0;
		op->rex_used = 0;
		op->val.prefix = p;
		op->tag.prefix = tag_const;
	}
}

	static void
DIS_CMPXCHG8B_Fixup (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	USED_REX (REX_W);
	if (bti386->rex & REX_W)
	{
    NOT_IMPLEMENTED();
		/* Change cmpxchg8b to cmpxchg16b.  */
		//char *p = (bti386->obuf) + strlen ((bti386->obuf)) - 2;
		//strlcpy (p, "16b", ((char *)(bti386->obuf) + sizeof (bti386->obuf)) - p);
		//bytemode = o_mode;
	}
	DIS_M (bti386, op, bytemode, sizeflag);
}

	static void
DIS_XMM_Fixup (bti386_t *bti386, operand_t *op, int reg, int sizeflag ATTRIBUTE_UNUSED)
{
	op->type = op_xmm;
	op->val.xmm = reg;
}

	static void
DIS_NOP_Fixup1 (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if ((bti386->prefixes & PREFIX_DATA) != 0
			|| (bti386->rex != 0
				&& bti386->rex != 0x48
				&& address_mode == mode_64bit)) {
		DIS_REG (bti386, op, bytemode, sizeflag);
	} else {
		//strcpy (obuf, "nop");
		DIS_REG(bti386, op, ax_reg, 2);
	}
}

	static void
DIS_NOP_Fixup2 (bti386_t *bti386, operand_t *op, int bytemode, int sizeflag)
{
	if ((bti386->prefixes & PREFIX_DATA) != 0
			|| (bti386->rex != 0
				&& bti386->rex != 0x48
				&& address_mode == mode_64bit)) {
		DIS_IMREG (bti386, op, bytemode, sizeflag);
	} else {
		DIS_REG(bti386, op, ax_reg, 2);
	}
}
