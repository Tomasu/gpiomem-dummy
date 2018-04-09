#KDIR ?= /lib/modules/$(shell uname -r)/build/
KDIR ?= /home/moose/build/linux-git/
INSN_PREFIX = "$(KDIR)/tools/objtool/arch/x86"

all:
	cp -a $(INSN_PREFIX)/lib/inat.c $(INSN_PREFIX)/lib/inat-tables.c $(INSN_PREFIX)/lib/insn.c .
	make -C $(KDIR) INSN_PREFIX=$(INSN_PREFIX) M=$(PWD) V=$V BCM2835_INC=$(BCM2835_INC) modules

clean:
	make -C $(KDIR) INSN_PREFIX=$(INSN_PREFIX) M=$(PWD) V=$V BCM2835_INC=$(BCM2835_INC) clean
