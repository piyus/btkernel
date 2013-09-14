#include <stdio.h>
#include <assert.h>

struct extable {
	unsigned long insn, fixup;
};

main()
{
  FILE *fp;
	struct extable ptr;
	fp = fopen("extable.txt", "rb");
	assert(fp);
	while (fread((void*)&ptr, sizeof(ptr), 1, fp)) {
		printf("insn=%lx fixup=%lx\n", ptr.insn, ptr.fixup);
	}
  fclose(fp);
}
