#ifndef SYS_VCPU_H
#define SYS_VCPU_H
#include <linux/types.h>
#include <types.h>
#include "peep/insntypes.h"
#include "sys/vcpu_consts.h"
//#include "sys/monitor.h"


/* Hidden flags. */
#define HF_CS32_SHIFT 1
#define HF_CS64_SHIFT 2


#define MAX_MEM_SIZE (0x400000)              //4 MB

typedef enum {
  cpu_mode_16 = 0,
  cpu_mode_32,
  cpu_mode_any,
} cpu_mode_t;

typedef struct vcpu_t {
  void *eip;
  unsigned long prev_tb;
  unsigned long edge;
	void *callout_label;
} vcpu_t;

static inline target_ulong
vcpu_get_eip(vcpu_t *vcpu)
{
  return (target_ulong)vcpu->eip;
}

#define get_sp_mask(e2) ({                  \
  long ret;                                 \
  if ((e2) & DESC_B_MASK) {                 \
    ret = 0xffffffff;                       \
  } else {                                  \
    ret = 0xffff;                           \
  }                                         \
  ret;                                      \
})

#define PUSHW(ssp, sp, sp_mask, val) do {                                      \
  sp -= 2;                                                                     \
  stw_kernel((ssp) + (sp & (sp_mask)), (val));                                 \
} while (0)

#define PUSHL(ssp, sp, sp_mask, val) do {                                      \
  sp -= 4;                                                                     \
  stl_kernel((ssp) + (sp & (sp_mask)), (val));                                 \
} while (0)

#define POPW(ssp, sp, sp_mask, val) do {                                      \
  val = lduw_kernel((ssp) + (sp & (sp_mask)));                                \
  sp += 2;                                                                    \
} while(0)

#define POPL(ssp, sp, sp_mask, val) do {                                      \
  val = (uint32_t)ldl_kernel((ssp) + (sp & (sp_mask)));                       \
  sp += 4;                                                                    \
} while(0)


#define SET_ESP(val, sp_mask) \
  vcpu.regs[R_ESP] = (vcpu.regs[R_ESP] & ~(sp_mask)) | ((val) & (sp_mask))



#define reset_stack() do {                                                    \
  asm("movl %0, %%esp ; movl %%esp, %%ebp ; movl $0x0, (%%ebp)" : :           \
      "g"((uint8_t *)thread_current() + PGSIZE -                              \
          2*sizeof(struct intr_handler_stack_frame)));                        \
} while(0)

#define ld(ptr, type, suffix) (*(type *)(ptr))
#define st(ptr, val, type, suffix) do { *(type *)(ptr) = (val); } while(0)

#define ld_kernel(ptr, type, suffix)  ({																			\
		type ret;																																	\
		pt_mode_t pt_mode;																												\
		/* ld_kernel may cause a TRACED page fault. Hence, it should always				\
		 * execute at cpl 3. */																										\
		ASSERT(read_cpl() == 3);																									\
		pt_mode = switch_to_shadow(0);																						\
		ret = ld(ptr, type, suffix);																							\
		switch_pt(pt_mode);																												\
		ret;																																			\
		})

#define st_kernel(ptr, val, type, suffix)  ({																	\
		pt_mode_t pt_mode;																												\
		/* st_kernel may cause a TRACED page fault. Hence, it should always				\
		 * execute at cpl 3. */																										\
		ASSERT(read_cpl() == 3);																									\
		pt_mode = switch_to_shadow(0);																						\
		st(ptr, val, type, suffix);																								\
		switch_pt(pt_mode);																												\
		})

#define ld_phys(ptr, type, suffix)  ({																				\
		type ret;																																	\
		pt_mode_t pt_mode;																												\
		pt_mode = switch_to_phys();																								\
		ret = ld(ptr, type, suffix);																							\
		switch_pt(pt_mode);																												\
		ret;																																			\
		})

#define st_phys(ptr, val, type, suffix)  ({																		\
		pt_mode_t pt_mode;																												\
		pt_mode = switch_to_phys();																								\
		st(ptr, val, type, suffix);																								\
		switch_pt(pt_mode);																												\
		})

static inline uint32_t
ldub(target_ulong ptr) {
	return ld(ptr, uint8_t, b);
}
static inline uint32_t
lduw(target_ulong ptr) {
	return ld(ptr, uint16_t, w);
}
static inline uint32_t
ldl(target_ulong ptr) {
	return ld(ptr, uint32_t, l);
}
static inline void
stub(target_ulong ptr, uint32_t val)	{
	st(ptr, val, uint8_t, b);
}
static inline void
stuw(target_ulong ptr, uint32_t val)	{
	st(ptr, val, uint16_t, w);
}
static inline void
stl(target_ulong ptr, uint32_t val)	{
	st(ptr, val, uint32_t, l);
}

#define ldub_kernel(ptr) 			ld_kernel(ptr, uint8_t, b)
#define lduw_kernel(ptr) 			ld_kernel(ptr, uint16_t, w)
#define ldl_kernel(ptr) 			ld_kernel(ptr, uint32_t, l)

#define stb_kernel(ptr, val) 	st_kernel(ptr, val, uint8_t, b)
#define stw_kernel(ptr, val) 	st_kernel(ptr, val, uint16_t, w)
#define stl_kernel(ptr, val) 	st_kernel(ptr, val, uint32_t, l)

#define ldub_phys(ptr) 				ld_phys(ptr, uint8_t, b)
#define lduw_phys(ptr) 				ld_phys(ptr, uint16_t, w)
#define ldl_phys(ptr) 				ld_phys(ptr, uint32_t, l)

#define stb_phys(ptr, val) 		st_phys(ptr, val, uint8_t, b)
#define stw_phys(ptr, val) 		st_phys(ptr, val, uint16_t, w)
#define stl_phys(ptr, val) 		st_phys(ptr, val, uint32_t, l)


uint32_t ldl_kernel_dont_set_access_bit(target_ulong vaddr);

#define CPU_INTERRUPT_HARD        0x02

#endif
