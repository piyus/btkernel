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
#include <string.h>
#include "bt/modconfig.h"

#define MAX_THREADS 255

void *
start_bt(void *arg)
{
  int fd = *((int*)arg);
  int id = *((int*)arg + 1);

  if (ioctl(fd, TRANSLATE_IDT, id) != 0) {
    perror("ioctl TRANSLATE_IDT error");
    abort();
  }
  return NULL;
}

static void
usage(void)
{
  printf("Usage: launch_bt [<mode>]. mode=0 (none), 1 (inc_icount), 2 (shadowmem)\n");
  exit(1);
}

int
main(int argc, char **argv)
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

  if (ioctl(fd, INIT_BT, n_procs) != 0) {
    perror("ioctl INIT_BT error");
    return 0;
  }

  if (argc > 1) {
    int mode;
    if (argc != 2) {
      usage();
    }
    mode = atoi(argv[1]);
    if (mode == 0 && strcmp(argv[1], "0")) {
      usage();
    }
    if (ioctl(fd, BT_SET_MODE, mode) != 0) {
      perror("ioctl BT_SET_MODE");
      return 0;
    }
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
    if (pthread_create(&threads[i], tattr, start_bt, arg)) {
      fprintf(stderr, "ERROR: Creating Pthread\n");
    }
  }
  for (i = 0; i < n_procs; i++) {
    pthread_join(threads[i], NULL);
  }
  close(fd);
  return 0;
}
