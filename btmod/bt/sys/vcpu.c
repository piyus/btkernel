#include <types.h>
#include "peep/tb.h"
#include "sys/monitor.h"
#include "sys/vcpu.h"
#include <linux/kernel.h>
#include <debug.h>
#include "hypercall.h"

target_ulong
vcpu_get_eip(vcpu_t *vcpu)
{
  return (target_ulong)(vcpu->segs[R_CS].base + (uint32_t)vcpu->eip);
}
/*
bool
vcpu_equal(vcpu_t const *cpu1, vcpu_t const *cpu2)
{
#define _compare_field(f, fmt, args...) do {                      \
  if (cpu1->f != cpu2->f) {                                       \
    dprintk("%s(): mismatch on " fmt ": %#x<->%#x\n", __func__,\
      ##args, (uint32_t)cpu1->f, (uint32_t)cpu2->f);             \
    return false;                                                 \
  }                                                               \
} while(0)

#define compare_field(f) do {       \
  _compare_field(f, #f);            \
} while(0)

#define compare_seg(s) do {             \
  compare_field(orig_##s);              \
  compare_field(s.base);                \
  compare_field(s.limit);               \
  compare_field(s.flags);               \
} while(0)

  //compare_field(n_exec);
  compare_field(eip);
  compare_field(regs[0]);
  compare_field(regs[1]);
  compare_field(regs[2]);
  compare_field(regs[3]);
  compare_field(regs[4]);
  compare_field(regs[5]);
  compare_field(regs[6]);
  compare_field(regs[7]);
  compare_seg(segs[0]);
  compare_seg(segs[1]);
  compare_seg(segs[2]);
  compare_seg(segs[3]);
  compare_seg(segs[4]);
  compare_seg(segs[5]);

  return true;
}
*/
/*
void
log_vcpu_state(vcpu_t *vcpu)
{
  dprintk("eip:%p ", vcpu->eip);
  dprintk("eax=%x ", vcpu->regs[0]);
  dprintk("ecx=%x ", vcpu->regs[1]);
  dprintk("edx=%x ", vcpu->regs[2]);
  dprintk("ebx=%x\n", vcpu->regs[3]);
  dprintk("esp=%x ", vcpu->regs[4]);
  dprintk("ebp=%x ", vcpu->regs[5]);
  dprintk("esi=%x ", vcpu->regs[6]);
  dprintk("edi=%x ", vcpu->regs[7]);
  dprintk("eflags=%x\n", vcpu->eflags);
}

void
log_monitor_state(monitor_t *monitor)
{
  dprintk("\n---logging monitor state---\n");
  dprintk("eip=%p ", monitor->eip);
  dprintk("eax=%x ", monitor->eax);
  dprintk("ecx=%x ", monitor->ecx);
  dprintk("edx=%x ", monitor->edx);
  dprintk("ebx=%x\n", monitor->ebx);
  dprintk("ebp=%x ", monitor->ebp);
  dprintk("esi=%x ", monitor->esi);
  dprintk("edi=%x ", monitor->edi);
  dprintk("eflags=%x\n", monitor->eflags);
}
*/
