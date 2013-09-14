
#include "bt/btmod.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sched.h>
#include <string.h>
#include "bt/config.h"
#include "bt/modconfig.h"

struct extable {
  unsigned long insn, fixup;
} table[2000];

int
main(int argc, char **argv)
{
#ifdef USER_EXTABLE
  int fd, i = 0, ret;
  char name[100];
	FILE *fp;
	struct extable_ioctl e;

  fp = fopen("extable.txt", "rb");
	assert(fp);
  snprintf(name, sizeof(name), "/dev/%s", MODULE_NAME);
  fd = open(name, O_RDONLY);
  if (fd < 0) {
    perror("unable to open dev");
    return 0;
  }

	memset((void*)table, 0, sizeof(table));
	while (fread(&table[i++], sizeof(struct extable), 1, fp) == 1) {
		assert(i <= 2000);
	}
	e.len = i - 1;
	e.addr = (unsigned)table;

  if (!ioctl(fd, INIT_EXTABLE, (unsigned long)&e)) {
    perror("Error ioctl\n");
		return 0;
  }
  close(fd);
	fclose(fp);
#endif
  return 0;
}
