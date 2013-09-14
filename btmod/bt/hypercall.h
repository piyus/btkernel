#ifndef __HYPERCALL_H__
#define __HYPERCALL_H__

#ifdef __PEEPGEN__

#include <stdio.h>
#define dprintk printf
#define myprintk printf

#else

#include <asm/io.h>
#include <linux/string.h>
#include <linux/module.h>
#include <asm/percpu.h>
#include "debug.h"
#include "config.h"


/* FIXME: Use Unused Port */
#define HYPERCALL_BENCH_PORT 0xa000
#define HYPERCALL_BT_PORT    0xa001
#define DUMP_PORT            0xa004
#define RESET_PORT           0xa005
#define RECORD_PORT          0xa007
#define GTABLE_PORT          0xa008

#define SWITCH_MODE  255
#define BUF_LEN      512

extern char __new_buf[2][512];
extern char __print_buf[512];

enum {
	PROFILE_START = 1,
  PROFILE_STOP,
	BENCH_PID,
	BENCH_VA,
	BT_PRINTK,
	RESET_BT,
	GLOBAL_INFO,
	RESET_GLOBAL_INFO,
};

#define start_bench(__name) \
({\
  int __i;\
  const char *__bench = __name;\
  for (__i = 0; __bench[__i] != '\0'; __i++) {\
	  outl(__bench[__i], HYPERCALL_BENCH_PORT);\
  }\
  outl(PROFILE_START, HYPERCALL_BENCH_PORT);\
})

#define stop_bench() \
({\
  outl(PROFILE_STOP, HYPERCALL_BENCH_PORT);\
})

#define start_remapping(_addr) \
({\
  outl(BENCH_VA, HYPERCALL_BT_PORT);\
	outl((_addr), HYPERCALL_BT_PORT);\
})

#define stop_remapping() {outl(RESET_BT, HYPERCALL_BT_PORT);}

#define set_global_info(__addr) \
({\
  outl(GLOBAL_INFO, HYPERCALL_BT_PORT);\
	outl((__addr), HYPERCALL_BT_PORT);\
})

#define reset_global_info() \
({\
  outl(RESET_GLOBAL_INFO, HYPERCALL_BT_PORT);\
})

#ifdef NO_PRINT
#define dprintk(...) {}
#else
#define dprintk(...) \
({\
  int __i, __n;\
  __n = snprintf(__print_buf, BUF_LEN, __VA_ARGS__);\
  if (__n < BUF_LEN - 1) {\
    outl(BT_PRINTK, HYPERCALL_BT_PORT);\
    for (__i = 0; __i < __n; __i++) {\
      outl(__print_buf[__i], HYPERCALL_BT_PORT);\
    }\
    outl(SWITCH_MODE, HYPERCALL_BT_PORT);\
  }\
})
#endif

#ifndef NO_PRINT

#define myprintk(...) \
({\
  int __i, __n;\
  char *__buf = __new_buf[v->vcpu_id];\
  __n = snprintf(__buf, BUF_LEN, __VA_ARGS__);\
  if (__n < BUF_LEN - 1) {\
    outl(BT_PRINTK, HYPERCALL_BT_PORT);\
    for (__i = 0; __i < __n; __i++) {\
      outl(__buf[__i], HYPERCALL_BT_PORT);\
    }\
    outl(SWITCH_MODE, HYPERCALL_BT_PORT);\
  }\
})

#else

#define myprintk(...) {}

#endif

#define tbprintk myprintk

#define safe_printk(...) \
({\
  /*spin_lock(&print_lock);*/\
  myprintk(__VA_ARGS__);\
  /*spin_unlock(&print_lock);*/\
})

#endif

#define must_printk(...) \
({\
  int __i, __n;\
  char *__buf = __new_buf[0/*v->vcpu_id*/];\
  __n = snprintf(__buf, BUF_LEN, __VA_ARGS__);\
  if (__n < BUF_LEN - 1) {\
    /*spin_lock(&print_lock);*/\
    outl(BT_PRINTK, HYPERCALL_BT_PORT);\
    for (__i = 0; __i < __n; __i++) {\
      outl(__buf[__i], HYPERCALL_BT_PORT);\
    }\
    outl(SWITCH_MODE, HYPERCALL_BT_PORT);\
    /*spin_unlock(&print_lock);*/\
  }\
})

#define MEMPANIC() \
	printk("PANIC: memory allocation failed %s %d\n", \
			__func__, __LINE__);

#endif
