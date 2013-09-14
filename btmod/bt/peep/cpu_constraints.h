#ifndef PEEP_CPU_CONSTRAINTS_H
#define PEEP_CPU_CONSTRAINTS_H
#include <linux/types.h>

typedef uint64_t cpu_constraints_t;

#define CPU_CONSTRAINT_REAL           (1 << 0)
#define CPU_CONSTRAINT_PROTECTED      (1 << 1)
#define CPU_CONSTRAINT_NO_EXCP        (1 << 2)
#define CPU_CONSTRAINT_GPF            (1 << 3)
#define CPU_CONSTRAINT_FORCED_CALLOUT (1 << 4)
#define CPU_CONSTRAINT_SIMULATE       (1 << 5)
#define CPU_CONSTRAINT_REMAPPED       (1 << 6)
#define DEFAULT_CPU_CONSTRAINTS \
  (CPU_CONSTRAINT_REAL | CPU_CONSTRAINT_PROTECTED | CPU_CONSTRAINT_NO_EXCP)

size_t cpu_constraints2str(cpu_constraints_t const *constraints, char *buf,
    size_t buf_size);
void str2cpu_constraints(char const *buf, cpu_constraints_t *constraints);

#endif
