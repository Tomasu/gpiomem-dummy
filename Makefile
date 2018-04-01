KDIR ?= /lib/modules/$(shell uname -r)/build/

all:
	make -C $(KDIR) M=$(PWD) V=$V modules

clean:
	make -C $(KDIR) M=$(PWD) V=$V clean
