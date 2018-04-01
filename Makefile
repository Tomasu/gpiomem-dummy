KDIR ?= /lib/modules/$(shell uname -r)/build/

all:
	make -C $(KDIR) M=$(PWD) V=$V BCM2835_INC=$(BCM2835_INC) modules

clean:
	make -C $(KDIR) M=$(PWD) V=$V BCM2835_INC=$(BCM2835_INC) clean
