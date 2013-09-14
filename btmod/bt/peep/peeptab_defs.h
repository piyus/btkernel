#ifndef PEEP_PEEPTAB_DEFS_H
#define PEEP_PEEPTAB_DEFS_H


#include "lib/macros.h"
#include "lib/mdebug.h"
#include "sys/vcpu_consts.h"
#include "config.h"
#include "header.h"

#define JUMPTABLE_OPTMIZATION 1

#ifndef __BTMOD__

#define INC_SREG0F \
  addl $C0, %fs:shadowregs;\
  adcl $0, %fs:(shadowregs+4)

#define INC_SREG0 \
  pushfl; \
  INC_SREG0F;\
  popfl

#define COUNT_LOADS \
	lock addl $1, C0

#define IDT_VECTOR \
  pushl %eax; \
  movl $C0, %eax;\
  outl $2;\
  popl %eax

#ifdef PROFILING

#define INC_SREG1F \
  addl $1, %fs:(shadowregs+8);\
  adcl $0, %fs:(shadowregs+12);

#define INC_SREG2F \
  addl $1, %fs:(shadowregs+16);\
  adcl $0, %fs:(shadowregs+20);

#define INC_SREG3F \
  addl $1, %fs:(shadowregs+24);\
  adcl $0, %fs:(shadowregs+28);

#else

#define INC_SREG1F
#define INC_SREG2F
#define INC_SREG3F

#endif

#define TWO_TEMPS_JUMP(target, temp1) \
  popl target;\
  popl temp1;\
  popfl;\
  jmpl *%fs:tx_target

#define ONE_TEMP_JUMP(temp1) \
  popl temp1;\
  popfl;\
  jmpl *%fs:tx_target

#define JUMP_INDIRECT_PART1(target, temp1) \
  movl target, temp1;\
  andl $JUMPTABLE1_MASK, temp1;\
  INC_SREG1F\
  cmpl target, %fs:jumptable1(,temp1,8);\
  jne 1f;\
  INC_SREG2F\
  movl %fs:jumptable1+4(,temp1,8), temp1;\
  movl temp1, %fs:tx_target


#define SEARCH_HASH_TABLE(target, temp1) \
  cmpl %fs:jumptable1+8(,temp1,8), target;\
  jne 2f;\
  INC_SREG3F\
  pushl target;\
  movl %fs:jumptable1(,temp1,8), target;\
  movl target, %fs:jumptable1+8(,temp1,8);\
  movl %fs:jumptable1+12(,temp1,8), target;\
  movl target, %fs:tx_target;\
  movl %fs:jumptable1+4(,temp1,8), target;\
  movl target, %fs:jumptable1+12(,temp1,8);\
  movl %fs:tx_target, target;\
  movl target, %fs:jumptable1+4(,temp1,8);\
  jmp 3f;\
2:  cmpl %fs:jumptable1+16(,temp1,8), target;\
  jne 2f;\
  INC_SREG3F\
  pushl target;\
  movl %fs:jumptable1(,temp1,8), target;\
  movl target, %fs:jumptable1+16(,temp1,8);\
  movl %fs:jumptable1+20(,temp1,8), target;\
  movl target, %fs:tx_target;\
  movl %fs:jumptable1+4(,temp1,8), target;\
  movl target, %fs:jumptable1+20(,temp1,8);\
  movl %fs:tx_target, target;\
  movl target, %fs:jumptable1+4(,temp1,8);\
  jmp 3f;\
2:  cmpl %fs:jumptable1+24(,temp1,8), target;\
  jne 1f;\
  INC_SREG3F\
  pushl target;\
  movl %fs:jumptable1(,temp1,8), target;\
  movl target, %fs:jumptable1+24(,temp1,8);\
  movl %fs:jumptable1+28(,temp1,8), target;\
  movl target, %fs:tx_target;\
  movl %fs:jumptable1+4(,temp1,8), target;\
  movl target, %fs:jumptable1+28(,temp1,8);\
  movl %fs:tx_target, target;\
  movl target, %fs:jumptable1+4(,temp1,8);\
3:  popl target;\
  movl target, %fs:jumptable1(,temp1,8)
 

#define INDIRECT_PART2(target) \
  set_jtarget_eip_noflags(target)

#ifdef JUMPTABLE_OPTMIZATION

#define JUMP_INDIRECT_2_TEMP(target, temp1) \
  JUMP_INDIRECT_PART1(target, temp1);\
  TWO_TEMPS_JUMP(target, temp1);\
1:  SEARCH_HASH_TABLE(target, temp1); \
  TWO_TEMPS_JUMP(target, temp1);\
1:  INDIRECT_PART2(target);\
  popl target;\
  popl temp1;\
  EDGE3;\
  EXIT_TB

#define JUMP_INDIRECT_1_TEMP(target, temp1) \
  JUMP_INDIRECT_PART1(target, temp1);\
  ONE_TEMP_JUMP(temp1);\
1:  SEARCH_HASH_TABLE(target, temp1);\
  ONE_TEMP_JUMP(temp1);\
1:  INDIRECT_PART2(target);\
  popl temp1;\
  EDGE3;\
  EXIT_TB

#else

#define JUMP_INDIRECT_2_TEMP(target, temp1) \
  JUMP_INDIRECT_PART1(target, temp1);\
  TWO_TEMPS_JUMP(target, temp1);\
1:  INDIRECT_PART2(target);\
  popl target;\
  popl temp1;\
  EDGE3;\
  EXIT_TB

#define JUMP_INDIRECT_1_TEMP(target, temp1) \
  JUMP_INDIRECT_PART1(target, temp1);\
  ONE_TEMP_JUMP(temp1);\
1:  INDIRECT_PART2(target);\
  popl temp1;\
  EDGE3;\
  EXIT_TB

#endif

#define TWO_TEMPS_CALL(target, temp1) \
  popl target;\
  popl temp1;\
  popfl;\
  calll *%fs:tx_target

#define ONE_TEMP_CALL(temp1) \
  popl temp1;\
  popfl;\
  calll *%fs:tx_target

#define CALL_INDIRECT_PART1(target, temp1) \
  movl target, temp1;\
  andl $JUMPTABLE1_MASK, temp1;\
  INC_SREG1F\
  cmpl target, %fs:jumptable1(,temp1,8);\
  jne end_Block;\
  INC_SREG2F\
  movl %fs:jumptable1+4(,temp1,8), temp1;\
  movl temp1, %fs:tx_target

#ifdef JUMPTABLE_OPTMIZATION

#define CALL_INDIRECT_2_TEMP(target, temp1) \
  CALL_INDIRECT_PART1(target, temp1);\
  TWO_TEMPS_CALL(target, temp1);\
.edge2: SEARCH_HASH_TABLE(target, temp1);\
  TWO_TEMPS_CALL(target, temp1);\
2: EMIT_EDGE1;\
1: INDIRECT_PART2(target);\
  popl %tr1d; popl %tr0d; popfl; \
  call 2f;\
  jmp 2b;\
2: pushfl; cli; pushl $0; EXIT_TB

#define CALL_INDIRECT_1_TEMP(target, temp1) \
  CALL_INDIRECT_PART1(target, temp1);\
  ONE_TEMP_CALL(temp1);\
.edge2: SEARCH_HASH_TABLE(target, temp1);\
  ONE_TEMP_CALL(temp1);\
2:  EMIT_EDGE1;\
1: INDIRECT_PART2(target);\
  popl %tr0d; popfl; \
  call 2f;\
  jmp 2b;\
2: pushfl; cli; pushl $0; EXIT_TB

#else

#define CALL_INDIRECT_2_TEMP(target, temp1) \
  CALL_INDIRECT_PART1(target, temp1);\
  TWO_TEMPS_CALL(target, temp1);\
.edge2: INDIRECT_PART2(target);\
  popl %tr1d; popl %tr0d; popfl; \
  call 2f;\
  EMIT_EDGE1;\
2: pushfl; cli; pushl $0; EXIT_TB

#define CALL_INDIRECT_1_TEMP(target, temp1) \
  CALL_INDIRECT_PART1(target, temp1);\
  ONE_TEMP_CALL(temp1);\
.edge2: INDIRECT_PART2(target);\
  popl %tr0d; popfl; \
  call 2f;\
  EMIT_EDGE1;\
2: pushfl; cli; pushl $0; EXIT_TB

#endif

#ifndef CALL_OPT

#define HANDLE_RET \
	popl %fs:tx_target;\
	pushfl;\
  cli;\
	pushl %edi;\
	pushl %esi;\
	movl %fs:tx_target, %esi;\
  JUMP_INDIRECT_2_TEMP(%esi, %edi)

#define HANDLE_CALL_C0 \
  pushl $fallthrough_addr;\
  jmp target_C0;\
  EDGE0($C0); EXIT_TB

#define HANDLE_CALL_REG \
  pushl $fallthrough_addr;\
	pushfl;\
  cli;\
	pushl %tr0d;\
  JUMP_INDIRECT_1_TEMP(%vr0d, %tr0d)

#define HANDLE_CALL_MEM \
  movl %tr1d, %fs:tx_target;\
  movl %vseg0:MEM32, %tr1d;\
  pushl $fallthrough_addr;\
	pushfl;\
  cli;\
	pushl %tr0d;\
  pushl %fs:tx_target;\
  JUMP_INDIRECT_2_TEMP(%tr1d, %tr0d)

#else

#define HANDLE_RET \
  ret

#define HANDLE_CALL_C0 \
  call end_Block;\
  EDGE2($C0); EXIT_TB

#define HANDLE_CALL_REG \
  pushfl;\
  cli;\
  pushl %tr0d;\
  CALL_INDIRECT_1_TEMP(%vr0d, %tr0d)

#define HANDLE_CALL_MEM \
  movl %tr1d, %fs:tx_target;\
  movl %vseg0:MEM32, %tr1d;\
  pushfl;\
  cli;\
  pushl %tr0d;\
  pushl %fs:tx_target;\
  CALL_INDIRECT_2_TEMP(%tr1d, %tr0d)

#endif

/* Snippets. */
#define SAVE_REG movl %vr0d, C0
#define LOAD_REG movl C0, %vr0d

#define set_eip(new_eip) \
	movl new_eip, (vcpu + VCPU_EIP_OFF)

#define set_jtarget_eip_noflags(new_eip) \
	movl new_eip, %fs:tx_target

#define set_jtarget_eip(new_eip) \
  pushfl;\
  cli;\
	movl new_eip, %fs:tx_target


#define _(x) 1f+(x); 1:

#define JUMP_TO_MONITOR jmp *(vcpu + VCPU_CALLOUT_LABEL_OFF)
#define JUMP_INDIR_INSN jmp *C0
#define EXIT_TB JUMP_TO_MONITOR

#define EDGE0(eip) .edge0: set_jtarget_eip(eip); pushl $joFF0;
#define EDGE1(eip) .edge1: set_jtarget_eip(eip); pushl $joFF1;
#define EDGE2(eip) .edge2: set_jtarget_eip(eip); pushl $joFF2;
#define EDGE3 pushl $0
#define SAVE_TEMPORARY(tnum)

#define RESTORE_TEMPORARY(tnum) 

#define MOV_MEM_TO_REG movl C0, %vr0d

#define MOVB_REGADDR_TO_AL  movb (%vr0d,%eiz,1), %al
#define MOVW_REGADDR_TO_AX  movw (%vr0d,%eiz,1), %ax
#define MOVL_REGADDR_TO_EAX movl (%vr0d,%eiz,1), %eax
#define MOVB_AL_TO_REGADDR  movb %al, (%vr0d,%eiz,1)
#define MOVW_AX_TO_REGADDR  movw %ax, (%vr0d,%eiz,1)
#define MOVL_EAX_TO_REGADDR movl %eax, (%vr0d,%eiz,1)

#define MOVB_DISPADDR_TO_REGADDR \
	movl C0, %vr1d;\
  movb (%vr1d,%eiz,1), %vr1b;\
  movb %vr1b, (%vr0d,%eiz,1)
#define MOVW_DISPADDR_TO_REGADDR \
	movl C0, %vr1d;\
  movw (%vr1d,%eiz,1), %vr1w;\
  movw %vr1w, (%vr0d,%eiz,1)
#define MOVL_DISPADDR_TO_REGADDR \
	movl C0, %vr1d;\
  movl (%vr1d,%eiz,1), %vr1d;\
  movl %vr1d, (%vr0d,%eiz,1)

#define EMIT_EDGE1 jmp tc_next_eip; EDGE1($fallthrough_addr); EXIT_TB
#define EMIT_EDGE0 jmp target_C0; EDGE0($fallthrough_addr); EXIT_TB

#define to_byte(x) x ## b
#define to_word(x) x ## w
#define to_long(x) x ## l
#define to_double(x) x ## d
#define glue(x, y, suffix) suffix(x ## y)

#define to_safe(x) x ## S
#define its_ok(x) x

#define SHADOWMEM_START 0x20000000

#define WRITE(addr, temp, _s) \
  _s(mov) %fs:_s(write__mask_), temp;\
  _s(or) temp, addr

#define READ(addr, temp, _s) \
  _s(mov) %fs:_s(read__mask_), temp;\
  _s(or) temp, addr

#define WRITES(addr, temp, _s) \
  leal addr, %tr0d;\
  cmpl $0xe0000000, %tr0d;\
  jb 99f;\
  WRITE(addr, temp, _s);\
99: nop


#define READS(addr, temp, _s) \
  leal addr, %tr0d;\
  cmpl $0xe0000000, %tr0d;\
  jb 99f;\
  READ(addr, temp, _s);\
99: nop

#define REMAP_WRITE(scale, suffix, tsuffix) \
	WRITE(C0(%vr0d,%vr1d,scale), tsuffix(%tr0), suffix)

#define RINDX_WRITE(scale, suffix, tsuffix) \
	WRITE(C0(,%vr0d,scale), tsuffix(%tr0), suffix)

#define RBASE_WRITE(suffix, tsuffix) \
	WRITE(C0(%vr0d,%eiz,1), tsuffix(%tr0), suffix)

#define RDISP_WRITE(suffix, tsuffix) \
	WRITE(C0, tsuffix(%tr0), suffix)

#define REMAP_READ(scale, suffix, tsuffix) \
	READ(C0(%vr0d,%vr1d,scale), tsuffix(%tr0), suffix)

#define RINDX_READ(scale, suffix, tsuffix) \
	READ(C0(,%vr0d,scale), tsuffix(%tr0), suffix)

#define RBASE_READ(suffix, tsuffix) \
	READ(C0(%vr0d,%eiz,1), tsuffix(%tr0), suffix)

#define RDISP_READ(suffix, tsuffix) \
	READ(C0, tsuffix(%tr0), suffix)

#define REMAPS_WRITE(scale, suffix, tsuffix) \
	WRITES(C0(%vr0d,%vr1d,scale), tsuffix(%tr0), suffix)

#define RINDXS_WRITE(scale, suffix, tsuffix) \
	WRITES(C0(,%vr0d,scale), tsuffix(%tr0), suffix)

#define RBASES_WRITE(suffix, tsuffix) \
	WRITES(C0(%vr0d,%eiz,1), tsuffix(%tr0), suffix)

#define RDISPS_WRITE(suffix, tsuffix) \
	WRITES(C0, tsuffix(%tr0), suffix)

#define REMAPS_READ(scale, suffix, tsuffix) \
	READS(C0(%vr0d,%vr1d,scale), tsuffix(%tr0), suffix)

#define RINDXS_READ(scale, suffix, tsuffix) \
	READS(C0(,%vr0d,scale), tsuffix(%tr0), suffix)

#define RBASES_READ(suffix, tsuffix) \
	READS(C0(%vr0d,%eiz,1), tsuffix(%tr0), suffix)

#define RDISPS_READ(suffix, tsuffix) \
	READS(C0, tsuffix(%tr0), suffix)

#define PUSH_TREG pushl %tr0d
#define POP_TREG popl %tr0d
#define PUSHFL pushfl
#define POPFL popfl

#define STOS(suffix, tsuffix, _f) \
	jecxz 1f;\
  jmp 3f;\
1: jmp 2f;\
3:	_f(WRITE)(C0(%edi,%eiz,1), tsuffix(%tr0), suffix);\
  suffix(stos);\
  subl $1, %ecx;\
  jnz 3b;\
2: nop

#define CMPS(suffix, tsuffix, _f) \
	jecxz 1f;\
  jmp 3f;\
1: jmp 2f;\
3:	_f(READ)(C0(%esi,%eiz,1), tsuffix(%tr0), suffix);\
	_f(READ)(C0(%edi,%eiz,1), tsuffix(%tr0), suffix);\
  suffix(cmps);\
  leal -1(%ecx), %ecx;\
  jecxz 2f;\
  jnz 3b;\
2: nop

#define SCAS(suffix, tsuffix, _f) \
	jecxz 1f;\
  jmp 3f;\
1: jmp 2f;\
3:	_f(READ)(C0(%edi,%eiz,1), tsuffix(%tr0), suffix);\
  suffix(scas);\
  leal -1(%ecx), %ecx;\
  jecxz 2f;\
  jnz 3b;\
2: nop

#define MOVS(suffix, tsuffix, _f) \
	jecxz 1f;\
  jmp 3f;\
1: jmp 2f;\
3:	_f(READ)(C0(%esi,%eiz,1), tsuffix(%tr0), suffix);\
	_f(WRITE)(C0(%edi,%eiz,1), tsuffix(%tr0), suffix);\
  suffix(movs);\
  subl $1, %ecx;\
  jnz 3b;\
2: nop

#define EMULATE_MOVS(suffix, tsuffix) \
	MOVS(suffix, tsuffix, its_ok)

#define EMULATE_MOVSS(suffix, tsuffix) \
	MOVS(suffix, tsuffix, to_safe)

#define EMULATE_STOS(suffix, tsuffix) \
	STOS(suffix, tsuffix, to_safe)

#define EMULATE_STOSS(suffix, tsuffix) \
	STOS(suffix, tsuffix, to_safe)

#define EMULATE_SCAS(suffix, tsuffix) \
	SCAS(suffix, tsuffix, its_ok)

#define EMULATE_SCASS(suffix, tsuffix) \
	SCAS(suffix, tsuffix, to_safe)

#define EMULATE_CMPS(suffix, tsuffix) \
	CMPS(suffix, tsuffix, its_ok)

#define EMULATE_CMPSS(suffix, tsuffix) \
	CMPS(suffix, tsuffix, to_safe)

#else

#define SAVE_GUEST \
	pusha

#define SAVE_GUEST_ESP \
  movl %esp, %fs:(gsptr)

#define RESTORE_GUEST_ESP \
  movl %fs:(gsptr), %esp

#define RESTORE_GUEST \
	popa;\
  addl $4, %esp;\
  popfl

#define SETUP_MONITOR_STACK \
  movl %__percpu_seg:(tsptr), %esp

#define JUMP_TO_TC \
  jmp *%fs:tx_target

#define jump_to_tc() \
  asm volatile (xstr(JUMP_TO_TC))

#define DEBUG_CODE \
  ud2

#define save_guest()     					  asm(xstr(SAVE_GUEST))
#define restore_guest()   					asm(xstr(RESTORE_GUEST))
#define setup_monitor_stack()       asm(xstr(SETUP_MONITOR_STACK))
#define debug_code()                asm(xstr(DEBUG_CODE))
#define save_guest_esp()            asm(xstr(SAVE_GUEST_ESP))
#define restore_guest_esp()         asm(xstr(RESTORE_GUEST_ESP))

#endif

#endif 
