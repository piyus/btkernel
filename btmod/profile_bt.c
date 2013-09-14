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
#include "bt/config.h"
#include "bt/modconfig.h"

#define MAX_THREADS 255

FILE *bench_fp;

void *
start_bt(void *arg)
{
  int fd = *((int*)arg);
  int id = *((int*)arg + 1);
  unsigned long long insns[260];
  if (ioctl(fd, READ_SHADOW_REG, (unsigned long)insns)) {
    perror("Error ioctl\n");
    return NULL;
  }
  fprintf(bench_fp, "PROCS=%d TOTAL=%lld INDIRECT=%lld JUMP1=%lld JUMP2=%lld BTMOD=%lld\n", 
    id, insns[0], insns[1], insns[2], insns[3], insns[4]);
  return NULL;
}

int
main(int argc, char **argv)
{
#if defined(INSCOUNT) || defined(PROFILING)
  pthread_t threads[MAX_THREADS];
  int fd;
  int n_procs;
  int i;
  int profile_stop;
  char bench_file[40], name[100];

  if (argc != 3) {
    printf("<usage> <0/1> <bench_name>\n");
    exit(0);
  }

#ifdef BENCH_FILE
  snprintf(bench_file, sizeof(bench_file), "/home/piyus/bench/%s.txt", BENCH_FILE);
#else
  snprintf(bench_file, sizeof(bench_file), "/home/piyus/bench/benchmark.txt");
#endif
  
  bench_fp = fopen(bench_file, "a");
  profile_stop = atoi(argv[1]);
  assert(profile_stop == 0 || profile_stop == 1);
  fprintf(bench_fp, "Benchmark %s ", argv[2]);
  (profile_stop)?fprintf(bench_fp, "Stopped\n"):fprintf(bench_fp, "Started\n");
  n_procs = sysconf(_SC_NPROCESSORS_ONLN);
  snprintf(name, sizeof(name), "/dev/%s", MODULE_NAME);
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
    if (pthread_create(&threads[i], tattr, start_bt, arg)) {
      fprintf(stderr, "ERROR: Creating Pthread\n");
    }
  }
  for (i = 0; i < n_procs; i++) {
    pthread_join(threads[i], NULL);
  }
  close(fd);
#endif
  return 0;
}
