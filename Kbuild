obj-m := gpiomem_dummy.o

gpiomem_dummy-objs := gpiomem_dummy_mod.o gpiomem_dummy_mmap.o gpiomem_dummy_procfs.o gpiomem_dummy_cdev.o gpiomem_dummy_probe.o
ccflags-y += -std=gnu11 -I$(INSN_PREFIX)/include
ccflags-$(CONFIG_GPIOMEM_DUMMY_DEBUG) += -g3 -DDEBUG
