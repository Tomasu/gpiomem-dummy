#ifndef GPIOMEM_PROCFS_H_GUARD
#define GPIOMEM_PROCFS_H_GUARD

#include <linux/proc_fs.h>

#define RANGES_SIZE 12

struct gpiomem_dummy_procfs {
   u32 mark_me;
   struct proc_dir_entry *proc_dt_ent;
   struct proc_dir_entry *proc_soc_ent;
   struct proc_dir_entry *proc_ranges_ent;
   u8 ranges_data[RANGES_SIZE];
};

extern int gpiomem_dummy_procfs_init(struct gpiomem_dummy_procfs *pfs);
extern void gpiomem_dummy_procfs_destroy(struct gpiomem_dummy_procfs *pfs);

#endif /* GPIOMEM_PROCFS_H_GUARD */
