#ifndef SYS_VCPU_CONSTS_H
#define SYS_VCPU_CONSTS_H

#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6
#define R_EDI 7

#define R_ES 0
#define R_CS 1
#define R_SS 2
#define R_DS 3
#define R_FS 4
#define R_GS 5

#define CR0_PE_MASK  (1 << 0)
#define CR0_MP_MASK  (1 << 1)
#define CR0_EM_MASK  (1 << 2)
#define CR0_TS_MASK  (1 << 3)
#define CR0_ET_MASK  (1 << 4)
#define CR0_NE_MASK  (1 << 5)
#define CR0_WP_MASK  (1 << 16)
#define CR0_AM_MASK  (1 << 18)
#define CR0_PG_MASK  (1 << 31)

#define EXCP00_DIVZ 0
#define EXCP01_SSTP 1
#define EXCP02_NMI  2
#define EXCP03_INT3 3
#define EXCP04_INTO 4
#define EXCP05_BOUND  5
#define EXCP06_ILLOP  6
#define EXCP07_PREX 7
#define EXCP08_DBLE 8
#define EXCP09_XERR 9
#define EXCP0A_TSS  10
#define EXCP0B_NOSEG  11
#define EXCP0C_STACK  12
#define EXCP0D_GPF  13
#define EXCP0E_PAGE 14
#define EXCP10_COPR 16
#define EXCP11_ALGN 17
#define EXCP12_MCHK 18

/* eflags masks */
#define CC_C    0x0001
#define CC_P  0x0004
#define CC_A  0x0010
#define CC_Z  0x0040
#define CC_S    0x0080
#define CC_O    0x0800

#define TF_SHIFT   8
#define IOPL_SHIFT 12
#define VM_SHIFT   17
#define IF_SHIFT   9
#define AC_SHIFT   18

#define TF_MASK     0x00000100
#define IF_MASK     0x00000200
#define DF_MASK     0x00000400
#define IOPL_MASK   0x00003000
#define NT_MASK     0x00004000
#define RF_MASK     0x00010000
#define VM_MASK     0x00020000
#define AC_MASK     0x00040000 
#define VIF_MASK                0x00080000
#define VIP_MASK                0x00100000
#define ID_MASK                 0x00200000

//#define GDT_SIZE 8192
//#define SEL_BASE ((GDT_SIZE*8) - SEL_SIZE)

#endif
