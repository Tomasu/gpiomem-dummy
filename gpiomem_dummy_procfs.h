#ifndef GPIOMEM_PROCFS_H_GUARD
#define GPIOMEM_PROCFS_H_GUARD

#include <linux/proc_fs.h>

struct gpiomem_dummy_procfs {
   struct proc_dir_entry *proc_fent;
};

int gpiomem_dummy_procfs_create(struct gpiomem_dummy_procfs *pfs);
void gpiomem_dummy_procfs_destroy(struct gpiomem_dummy_procfs *pfs);

#endif /* GPIOMEM_PROCFS_H_GUARD */
