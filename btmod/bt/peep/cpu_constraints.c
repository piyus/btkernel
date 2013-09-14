#include <types.h>
#include "peep/cpu_constraints.h"
#include "sys/vcpu.h"
#include <linux/kernel.h>
#include <linux/string.h>
#include <debug.h>
#include "hypercall.h"

cpu_constraints_t const constraint_bits[] = {
  CPU_CONSTRAINT_REAL,
  CPU_CONSTRAINT_PROTECTED,
  CPU_CONSTRAINT_NO_EXCP,
  CPU_CONSTRAINT_GPF,
  CPU_CONSTRAINT_FORCED_CALLOUT,
	CPU_CONSTRAINT_SIMULATE,
	CPU_CONSTRAINT_REMAPPED,
};

static char const *
constraint_bit2str(cpu_constraints_t constraint)
{
  switch (constraint) {
    case CPU_CONSTRAINT_REAL:
      return "real";
    case CPU_CONSTRAINT_PROTECTED:
      return "protected";
    case CPU_CONSTRAINT_NO_EXCP:
      return "no_excp";
    case CPU_CONSTRAINT_GPF:
      return "gpf";
    case CPU_CONSTRAINT_FORCED_CALLOUT:
      return "forced_callout";
    case CPU_CONSTRAINT_SIMULATE:
      return "simulate";
    case CPU_CONSTRAINT_REMAPPED:
      return "remapped";
    default:
      NOT_REACHED();
  }
  return NULL;
}

#if 0
static cpu_constraints_t const *
cpu_constraints_current(void)
{
  static cpu_constraints_t ret;
  ret = 0;
  if (vcpu.cr[0] & CR0_PE_MASK) {
    ret |= CPU_CONSTRAINT_PROTECTED;
  } else {
    ret |= CPU_CONSTRAINT_REAL;
  }
  return &ret;
}
#endif

size_t
cpu_constraints2str(cpu_constraints_t const *constraints, char *buf,
    size_t buf_size)
{
  char *ptr = buf, *end = buf + buf_size;
  unsigned i;

  for (i = 0; i < sizeof constraint_bits/sizeof constraint_bits[0]; i++) {
    if (*constraints & constraint_bits[i]) {
      ptr += snprintf(ptr, end - ptr, " %s",
          constraint_bit2str(constraint_bits[i]));
    }
    ASSERT(ptr < end);
  }
  return ptr - buf;
}

void
str2cpu_constraints(char const *buf, cpu_constraints_t *constraints)
{
  char const *ptr = buf;
  char constraint[32];
  size_t num_read;
  *constraints = 0;
  while (sscanf(ptr, "%s%n", constraint, &num_read) > 0) {
    bool done = false;
    unsigned i;
    ASSERT(num_read < sizeof constraint);
    for (i = 0; i < sizeof constraint_bits/sizeof constraint_bits[0]; i++) {
      if (!strcmp(constraint, constraint_bit2str(constraint_bits[i]))) {
        done = true;
        *constraints = *constraints | constraint_bits[i];
      }
    }
    ASSERT(done);
    ptr += num_read;
  }
}
