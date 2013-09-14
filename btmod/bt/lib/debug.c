#include <debug.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/string.h>

int dbg_level = 1;
int loglevel;

vcpu_log_item_t vcpu_log_items[] = {
  { VCPU_LOG_OUT_ASM, "out_asm",
    "show generated host assembly code for each compiled TB" },
  { VCPU_LOG_IN_ASM, "in_asm",
    "show target assembly code for each compiled TB, and the peep rules triggered" },
  { VCPU_LOG_HW, "hw", "used to print hardware debug messages" },
  { VCPU_LOG_INT, "int", "show interrupts" },
  { VCPU_LOG_EXCP, "excp", "show exceptions" },
  { VCPU_LOG_USB, "usb", "show usb driver debug info"},
  { VCPU_LOG_CPU, "cpu", "show CPU state before block translation" },
  { VCPU_LOG_PCALL, "pcall","show protected mode far calls/returns/exceptions"},
  { VCPU_LOG_IOPORT, "ioport", "show all i/o ports accesses" },
  { VCPU_LOG_TRANSLATE, "translate", "show all translations" },
  { VCPU_LOG_MTRACE, "mtrace", "show mtraces" },
  { VCPU_LOG_PAGING, "paging", "show paging activity" },
  { VCPU_LOG_TB, "translation-blocks", "show translation cache activity" },
  { 0, NULL, NULL },
};

/* enable or disable low level log */
void vcpu_set_log(int log_flags)
{
  loglevel |= log_flags;
}

void vcpu_clear_log(int log_flags)
{
  loglevel &= ~log_flags;
}

int vcpu_get_log_flags(void)
{
	return loglevel;
}


#if 0

/* Prints the call stack, that is, a list of addresses, one in
   each of the functions we are nested within.  gdb or addr2line
   may be applied to kernel.o to translate these into file names,
   line numbers, and function names.  */
void
debug_backtrace (void **frame_address) 
{
  void **frame;
	//void **__intr_handler_stack_frame = NULL;
	int num_back_jumps = 0;
#define PRINT(...) do {printf(__VA_ARGS__);}while(0)
  
  PRINT("Call stack:");
  for (frame = frame_address;
       frame != NULL && frame[0] != NULL && frame[1] != (void *)0xffffffff
			 && frame[1] != NULL
       ; frame = frame[0]) {
    PRINT(" %p", frame[1]);
#ifdef __MONITOR__
#if 0
		if ((uint8_t *)frame[0] >= (uint8_t *)thread_current() + PGSIZE
											- 2*sizeof(struct intr_handler_stack_frame)) {
			/* This is where we are jumping back to intr_entry. Get the user stack
			 * from here from intr_frame->ebp. */
			__intr_handler_stack_frame = frame;
		}
#endif
#endif
    if (frame[0] <= (void *)frame) {
			/*
      PRINT("\nNew frame pointer[%p] <= current frame pointer[%p]. "
          "Stopping...\n", frame[0], frame);
					*/
			if (++num_back_jumps == 2) {
				PRINT("\nNew frame pointer[%p] <= current frame pointer[%p]. "
						"Stopping...\n", frame[0], frame);
				break;
			}
    }
  }
#if 0
	if (__intr_handler_stack_frame) {
		struct intr_frame *intr_frame = __intr_handler_stack_frame[2];
		for (frame = (void **)intr_frame->ebp;
				frame != NULL && frame[0] != NULL
				&& frame >= (void **)LOADER_MONITOR_VIRT_BASE;
				frame = frame[0]) {
			PRINT(" %p", frame[1]);
			if (frame[0] <= (void *)frame) {
				PRINT("\nNew frame pointer[%p] <= current frame pointer[%p]. "
						"Stopping...\n", frame[0], frame);
				break;
			}
		}
	}
#endif
  PRINT(".\n");

	num_back_jumps = 0;
  PRINT("Call stack pointers:");
  for (frame = frame_address; /*__builtin_frame_address (0);*/
       frame != NULL && frame[0] != NULL && frame[1] != NULL;
       frame = frame[0]) {
    PRINT(" %p", frame);
    if (frame[0] <= (void *)frame) {
			if (++num_back_jumps == 2) {
				break;
			}
      break;
    }
  }
  PRINT(".\n");
#undef PRINT
}

/* Halts the OS, printing the source file name, line number, and
   function name, plus a user-specific message. */
void
debug_panic (const char *file, int line, const char *function,
             const char *message, ...)
{
  static int level = 0;
  va_list args;
	static uint32_t esp;

	//console_panic();
  level++;
	asm volatile ("movl %%esp, %0" : "=g"(esp));
  if (level == 1) {
    debug_backtrace (__builtin_frame_address(0));
    printf ("Kernel PANIC at %s:%d in %s() [esp %x]: ", file, line, function,
				esp);

    va_start (args, message);
    vprintf (message, args);
    printf ("\n");
    va_end (args);

    debug_backtrace (__builtin_frame_address(0));
#ifdef __MONITOR__
    record_log_panic();
#endif
  } else if (level == 2) {
    printf ("Kernel PANIC recursion at %s:%d in %s().\n",
        file, line, function);
  } else {
    /* Don't print anything: that's probably why we recursed. */
  }

  //intr_disable();
  serial_flush ();
	shutdown_power_off();
	NOT_REACHED();
}

/* Print call stack of a thread.
   The thread may be running, ready, or blocked. */
static void
print_stacktrace(struct thread *t, void *aux UNUSED)
{
  void *retaddr = NULL, **frame = NULL;
  const char *status = "UNKNOWN";

  switch (t->status) {
    case THREAD_RUNNING:  
      status = "RUNNING";
      break;

    case THREAD_READY:  
      status = "READY";
      break;

    case THREAD_BLOCKED:  
      status = "BLOCKED";
      break;

    default:
      break;
  }

  printf ("Call stack of thread `%s' (status %s):", t->name, status);

  if (t == thread_current()) 
    {
      frame = __builtin_frame_address (1);
      retaddr = __builtin_return_address (0);
    }
  else
    {
      /* Retrieve the values of the base and instruction pointers
         as they were saved when this thread called switch_threads. */
      struct switch_threads_frame * saved_frame;

      saved_frame = (struct switch_threads_frame *)t->stack;

      /* Skip threads if they have been added to the all threads
         list, but have never been scheduled.
         We can identify because their `stack' member either points 
         at the top of their kernel stack page, or the 
         switch_threads_frame's 'eip' member points at switch_entry.
         See also threads.c. */
      if (t->stack == (uint8_t *)t + PGSIZE || saved_frame->eip == switch_entry)
        {
          printf (" thread was never scheduled.\n");
          return;
        }

      frame = (void **) saved_frame->ebp;
      retaddr = (void *) saved_frame->eip;
    }

  printf (" %p", retaddr);
  for (; (uintptr_t) frame >= 0x1000 && frame[0] != NULL; frame = frame[0])
    printf (" %p", frame[1]);
  printf (".\n");
}

/* Prints call stack of all threads. */
void
debug_backtrace_all (void)
{
  enum intr_level oldlevel = intr_disable ();

  thread_foreach (print_stacktrace, 0);
  intr_set_level (oldlevel);
}

static int
cmp1(const char *s1, unsigned n, const char *s2)
{
  if (strlen(s2) != n) {
    return 0;
  }
  return memcmp(s1, s2, n) == 0;
}

/* takes a comma separated list of log masks. Return 0 if error. */
int vcpu_str_to_log_mask(const char *str)
{
  vcpu_log_item_t const *item;
  int mask;
  char const *p, *p1;

  p = str;
  mask = 0;
  for(;;) {
    p1 = strchr(p, ',');
    if (!p1) {
      p1 = p + strlen(p);
    }
    if (cmp1(p, p1 - p, "all")) {
      for(item = vcpu_log_items; item->mask != 0; item++) {
        mask |= item->mask;
      }
    } else {
      for(item = vcpu_log_items; item->mask != 0; item++) {
        if (cmp1(p, p1 - p, item->name)) {
          goto found;
        }
      }
      return 0;
    }
found:
    mask |= item->mask;
    if (*p1 != ',') {
      break;
    }
    p = p1 + 1;
  }
  return mask;
}

#endif
