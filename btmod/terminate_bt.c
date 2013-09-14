#define _GNU_SOURCE

#include "bt/btmod.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sched.h>
#include "bt/modconfig.h"

#define MAX_THREADS 255

static void *
load_native_idt(void *arg)
{
  int fd = *((int*)arg);
  int idx = *((int*)arg + 1);
  if (ioctl(fd, LOAD_NATIVE_IDT, idx) != 0) {
    perror("ioctl LOAD_NATIVE_IDT");
  }
  return NULL;
}

int
main(void)
{
  pthread_t threads[MAX_THREADS];
  char name[100];
  int fd;
  int n_procs;
  int i;

  snprintf(name, sizeof(name), "/dev/%s", MODULE_NAME);
  n_procs = sysconf(_SC_NPROCESSORS_ONLN);
  fd = open(name, O_RDONLY);
  if (fd < 0) {
    perror("unable to open dev");
    return 0;
  }

  for (i = 0; i < n_procs; i++) {
    int *arg = malloc(sizeof(int)*2);
    pthread_attr_t *tattr = (pthread_attr_t *)malloc(sizeof(pthread_attr_t));
    cpu_set_t cpuset;
    assert(arg);
    *arg = fd;
    *(arg + 1) = i;
    assert(tattr);
    pthread_attr_init(tattr);
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    pthread_attr_setaffinity_np (tattr, sizeof(cpuset), &cpuset);
    if (pthread_create(&threads[i], tattr, load_native_idt, arg)) {
      fprintf(stderr, "ERROR: Creating Pthread\n");
      close(fd);
      return 0;
    }
  }
  for (i = 0; i < n_procs; i++) {
    pthread_join(threads[i], NULL);
  }
  if (ioctl(fd, STOP_BT, n_procs) != 0) {
    perror("ioctl STOP_BT");
  }
  close(fd);
  return 0;
}
