
PWD ?= $(shell pwd)
MODULE_NAME=bt

IDIR ?= -I$(PWD)/lib -I$(PWD)/mem -I$(PWD)/peep -I$(PWD)/sys -I$(PWD) -I$(PWD)/include
ifneq ($(KERNELRELEASE),)

EXTRA_CFLAGS += $(IDIR) -g -O0
EXTRA_CFLAGS += -D__BTMOD__ 

MON_OBJS := lib/mylist.o lib/hash.o lib/rbtree.o lib/utils.o lib/debug.o \
				 		peep/tb.o peep/i386-dis.o peep/insn.o peep/opctable.o \
					  peep/regset.o peep/nomatch_pair.o peep/jumptable2.o \
						peep/jumptable1.o peep/cpu_constraints.o peep/peepcode.o \
            peep/assignments.o peep/peep.o \
            peep/instrument.o  mem/malloc.o \
						btfix.o btmod.o bt_main.o


$(MODULE_NAME)-y += $(MON_OBJS)

obj-m	+= $(MODULE_NAME).o

else

KDIR ?= /lib/modules/`uname -r`/build

default: peepgen_entries.h peepgen_defs.h peepgen_gencode.h
	$(MAKE) -C $(KDIR) M=$$PWD modules

PEEPGEN_SRCS = peepgen.c lib/utils.c peep/insn.c peep/assignments.c \
							 peep/cpu_constraints.c peep/nomatch_pair.c \
							 peep/i386-dis.c lib/mylist.c lib/hash.c peep/opctable.c \
							 peep/regset.c lib/debug.c

#peep/peep.o:
peepgen_offsets.h: peepgen sys/vcpu.h
	./peepgen -f
%.s: %.S
	$(CC) -P -E $< > $@ $(IDIR)

peep/peeptab_defs.h: peepgen_offsets.h
peep.S: peep.tab peep/peeptab_defs.h peepgen peep/snippets.h
	cp peep.tab peep.S && ./peepgen -s peep/snippets.h -a peep.S
in.S out.S vars: peepgen peep.s peep/peeptab_defs.h
	sed "s/\s*>>/\n/g" peep.s > peep.es
	./peepgen peep.es
in.o out.o: %.o: %.s as
	./as --32 $< -o $@
peepgen_entries.h: peepgen in.o vars.ordered nomatch_pairs
	./peepgen -t -o $@ > opctable
vars.ordered: peepgen in.o vars
	./peepgen -v -o $@
peepgen_defs.h: peepgen in.o vars
	./peepgen -d -o $@
peepgen_gencode.h: peepgen out.o vars.ordered
	./peepgen -g -o $@
nomatch_pairs: peepgen out.o vars
	./peepgen -n -o $@
peepgen: $(PEEPGEN_SRCS)
	$(CC) -m32 -g -D__PEEPGEN__ -o peepgen $(PEEPGEN_SRCS) $(IDIR)

as: ../binutils/install/bin/as
	ln -sf $< $@

BINUTILS_DIST=binutils-2.19
BINUTILS_DIST_PATH=../$(BINUTILS_DIST).tar.bz2

../binutils/install/bin/as: $(PWD)/misc/binutils-2.19.patch
	mkdir -p ../binutils
	rm -rf ../binutils/$(BINUTILS_DIST)
	tar xjf $(BINUTILS_DIST_PATH) -C ../binutils
	cd ../binutils/$(BINUTILS_DIST) && patch -p1 < $<
	cd ../binutils/$(BINUTILS_DIST) \
		&& sh myconfigure --prefix=`pwd`/../install
	make -C ../binutils/$(BINUTILS_DIST)
	make -C ../binutils/$(BINUTILS_DIST) install


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf `find -iname '*.o'` `find -iname '*~'` peepgen in.[sS] out.[sS] vars vars.ordered \
	  peepgen_gencode.h peepgen_defs.h peepgen_entries.h nomatch_pairs peep/peepgen_outcode.S \
	  peepgen_offsets.h peep.S peep.es cpu_constraints gen_snippets.h as out opctable


endif
