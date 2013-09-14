#ifndef __LIB_DEBUG_H
#define __LIB_DEBUG_H
#include "hypercall.h"

#ifdef __PEEPGEN__
#define dump_stack() 1
#endif

#define UNUSED __attribute__ ((unused))
#define NO_RETURN __attribute__ ((noreturn))
#define NO_INLINE __attribute__ ((noinline))
#define PRINTF_FORMAT(FMT, FIRST) __attribute__ ((format (dprintk, FMT, FIRST)))

#define PANIC(...) {dprintk(__VA_ARGS__);}

extern int dbg_level;
#ifndef NDEBUG
#define DBGn(n,x,args...) do {                                                \
  if (dbg_level >= n) {                                                       \
    dprintk(x, ##args);                                                        \
  }                                                                           \
} while(0)
#define DBE(l,x) if (dbg_level >= l) do { x; } while(0)
#define DBG_(x,args...) DBGn(1, "[%s() %d] " x, __func__, __LINE__, ##args)
#define DBG(x,args...) DBGn(1, x, ##args)
#define ERR(x,args...) do {                                                   \
  DBGn(0,"Error at %s:%d\n%s(): " x, __FILE__, __LINE__, __func__, ##args);   \
} while(0)
#define MSG(x, args...) dprintk(x, ##args)
#else
#define DBGn(n,x,...)
#define DBE(l,x)
#define DBG_(x,...)
#define DBG(x,...)
#define ERR(x,...)
#define MSG(...)
#endif


#define VCPU_LOG_ALWAYS        (1 << 0)
#define VCPU_LOG_OUT_ASM       (1 << 1)
#define VCPU_LOG_IN_ASM        (1 << 2)
#define VCPU_LOG_HW            (1 << 3)
#define VCPU_LOG_INT           (1 << 4)
#define VCPU_LOG_EXCP          (1 << 5)
#define VCPU_LOG_USB           (1 << 6)
#define VCPU_LOG_PCALL         (1 << 7)
#define VCPU_LOG_IOPORT        (1 << 8)
#define VCPU_LOG_CPU           (1 << 9)
#define VCPU_LOG_TRANSLATE     (1 << 10)
#define VCPU_LOG_MTRACE        (1 << 11)
#define VCPU_LOG_PAGING        (1 << 12)
#define VCPU_LOG_TB 		       (1 << 13)

#define INSN 2
#define MATCH 2
#define MATCH_ALL 2

extern int loglevel;

#define LOG(n,x,args...) do {                                               \
  if (loglevel & VCPU_LOG_##n) {                                            \
    dprintk(x, ##args);                                                      \
  }                                                                         \
} while(0)


typedef struct vcpu_log_item_t {
  int mask;
  char const *name;
  char const *help;
} vcpu_log_item_t ;

extern vcpu_log_item_t vcpu_log_items[];
void vcpu_set_log(int log_flags);
int vcpu_get_log_flags(void);
void vcpu_clear_log(int log_flags);
int vcpu_str_to_log_mask(char const *str);

#endif  /* lib/debug.h */


/* This is outside the header guard so that debug.h may be
   included multiple times with different settings of NDEBUG. */
//#undef ASSERT
//#undef WARN_ON
//#undef NOT_REACHED
//#undef NOT_IMPLEMENTED
//#undef NOT_TESTED

#define MASSERT(CONDITION...)                                 \
        if ((CONDITION)) { } else {                               \
          dprintk("Assert `%s' failed. %s %d\n", #CONDITION, __func__, __LINE__);   \
        }

#ifndef NDEBUG
#ifndef ASSERT
#define ASSERT(CONDITION...)                                 \
        if ((CONDITION)) { } else {                               \
          PANIC ("Assert `%s' failed. %s %d\n", #CONDITION, __func__, __LINE__);   \
        }
#endif
#define ASSERT2 ASSERT
#else
#define ASSERT(CONDITION...) ((void) 0)
#define ASSERT2(CONDITION...) ((void) 0)
#endif
#define NOT_REACHED() PANIC ("%s", "executed an unreachable statement");
#define NOT_IMPLEMENTED() PANIC ("%s", "not-implemented");
#define NOT_TESTED() do {																					\
	dprintk("%s() %d: Not tested.\n", __func__, __LINE__);					\
	debug_backtrace(__builtin_frame_address(0));										\
} while(0)

#define ABORT() {PANIC("aborting %s %d\n", __func__, __LINE__);}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define prefetch(x) __builtin_prefetch(x)

