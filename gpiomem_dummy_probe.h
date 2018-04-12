#ifndef GPIOMEM_DUMMY_PROBE_H_GUARD
#define GPIOMEM_DUMMY_PROBE_H_GUARD

#include <linux/list.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/fs.h>
#include <linux/perf_event.h>

struct gpiomem_dummy;
struct gd_probe_info {
   struct list_head list; // list pointers

   // what do we want here??
   unsigned long ip;
   struct inode *inode;
   struct task_struct *task;
   struct vm_area_struct *vma; // ??
   struct perf_event *perf_event;
};

struct gd_probe_info *gd_create_probe(struct task_struct *task, struct vm_area_struct *vma);


int gd_register_probe(struct task_struct *task, struct vm_area_struct *vma);

void gd_add_probe(struct gpiomem_dummy *gd, struct gd_probe_info *probe);

#endif /* GPIOMEM_DUMMY_PROBE_H_GUARD */
