obj-m := gpiomem_dummy.o
gpiomem_dummy-y := gpiomem_dummy_mod.o gpiomem_dummy_mmap.o gpiomem_dummy_procfs.o gpiomem_dummy_cdev.o
ccflags-$(CONFIG_GPIOMEM_DUMMY_DEBUG) += -g3 -DDEBUG
