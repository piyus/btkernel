#ifndef PEEP_PEEPGEN_H
#define PEEP_PEEPGEN_H

/* VAR_CONST(i) must be between -127 and 128 (loop instruction). */
#define VAR_CONST(i) (0x70+(i))
#define CONST_VAR(i) ((i) - 0x70)
#define MAX_IMM_VARS 8

#endif
