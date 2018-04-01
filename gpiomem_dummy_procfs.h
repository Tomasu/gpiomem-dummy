#ifndef GPIOMEM_PROCFS_H_GUARD
#define GPIOMEM_PROCFS_H_GUARD

#include <linux/proc_fs.h>

struct gpiomem_dummy_procfs {
   struct proc_dir_entry *proc_dent;
   struct proc_dir_entry *proc_fent;
};

extern int gpiomem_dummy_procfs_init(struct gpiomem_dummy_procfs *pfs);
extern void gpiomem_dummy_procfs_destroy(struct gpiomem_dummy_procfs *pfs);

#endif /* GPIOMEM_PROCFS_H_GUARD */
