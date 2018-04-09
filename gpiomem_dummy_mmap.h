#ifndef GPIOMEM_DUMMY_MMAP_H_GUARD
#define GPIOMEM_DUMMY_MMAP_H_GUARD

#include <linux/mm_types.h>
#include <linux/mm.h>

extern struct vm_operations_struct gpiomem_dummy_mmap_vmops;

#endif /* GPIOMEM_DUMMY_MMAP_H_GUARD */
