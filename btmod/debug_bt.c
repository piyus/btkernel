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

void *
start_bt(void *arg)
{
  int fd = *((int*)arg);
  int id = *((int*)arg + 1);

  if (ioctl(fd, TRANSLATE_DEBUG_CODE, id) != 0) {
    perror("ioctl TRANSLATE_DEBUG_CODE");
    abort();
  }
  return NULL;
}

int
main(void)
{
  pthread_t threads[MAX_THREADS];
  int fd;
  int n_procs;
  int i;

  n_procs = sysconf(_SC_NPROCESSORS_ONLN);
  fd = open("/dev/bt", O_RDONLY);
  if (fd < 0) {
    perror("unable to open dev");
    return 0;
  }

  /*if (ioctl(fd, INIT_BT, n_procs) != 0) {
    perror("ioctl INIT_BT");
    return 0;
  }*/

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
