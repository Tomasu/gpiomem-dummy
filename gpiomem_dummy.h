#ifndef GPIOMEM_DUMMY_H_GUARD
#define GPIOMEM_DUMMY_H_GUARD

#include "gpiomem_dummy_cdev.h"
#include "gpiomem_dummy_procfs.h"
#include "gpiomem_dummy_pdev.h"

#define DEVICE_NAME "gpiomem"    ///< The device will appear at /dev/gpiomem using this value
#define CLASS_NAME  "gpiomem"        ///< The device class -- this is a character device driver

#define LOG_PREFIX_ "gpiomem-dummy-"

#define BCM283X_PERIPH_BASE 0x7E000000Lu
#define BCM283X_PERIPH_PGOFF (BCM283X_PERIPH_BASE >> PAGE_SHIFT)
#define BCM283X_PERIPH_SIZE 0x1000000Lu

#define BCM283X_GPIO_OFFSET 0x200000Lu
#define BCM283X_GPIO_BASE (BCM283X_PERIPH_BASE + BCM283X_GPIO_OFFSET)
#define BCM283X_GPIO_SIZE 0xb4Lu

#define BCM283X_GPIO_PGOFF (BCM283X_GPIO_OFFSET >> PAGE_SHIFT)

#define KALLOC_MEM_SIZE PAGE_SIZE

#include <linux/mm_types.h>


struct gpiomem_dummy
{
   int initialized;
   struct gpiomem_dummy_cdev cdev; // our char device state.
   struct gpiomem_dummy_procfs proc; // our proc device state;

   struct page *page; // our page!

   struct list_head probe_list; // list of tasks (gd_probe_info) that have mmaped us
};

#define check_error(thing, fmt, ...) do { \
typeof(thing) thing_copy_ = (thing); \
if(!thing_copy_ || IS_ERR(thing_copy_)) \
{ \
   pr_err("check failed: " #thing " != NULL || IS_ERR(" #thing ")"); \
   pr_err(fmt, ##__VA_ARGS__); \
   return PTR_ERR(thing_copy_); \
} \
} while(0)

#define check_error_cleanup(thing, fmt, ...) do { \
typeof(thing) thing_copy_ = (thing); \
if(!thing_copy_ || IS_ERR(thing_copy_)) \
{ \
   pr_err("check failed: " #thing " != NULL || IS_ERR(" #thing ")"); \
   pr_err(fmt, ##__VA_ARGS__); \
   error_ret = PTR_ERR(thing_copy_); \
   goto err_cleanup; \
} \
} while(0)

#define check_state_cleanup(val, errcode, fmt, ...) do { \
typeof(val) val_copy_ = (val); \
if(val_copy_) \
{ \
   break; \
} \
\
pr_err("check failed: " #val); \
pr_err(fmt, ##__VA_ARGS__); \
error_ret = errcode; \
goto err_cleanup; \
} while(0)

#define check_val_cleanup(val, fmt, ...) do { \
typeof(val) val_copy_ = (val); \
if(val_copy_ == 0) \
{ \
   break; \
} \
\
pr_err("check failed: " #val " == 0"); \
pr_err(fmt, ##__VA_ARGS__); \
error_ret = val_copy_; \
goto err_cleanup; \
} while(0)

int gd_set_page_ro(struct page *page);
int gd_set_page_rw(struct page *page);
struct gpiomem_dummy *gd_get(void);

static inline
struct gpiomem_dummy *gd_cdev_get_dummy(struct gpiomem_dummy_cdev *cd)
{
   return container_of(cd, struct gpiomem_dummy, cdev);
}


static inline
struct gpiomem_dummy *gd_procfs_get_dummy(struct gpiomem_dummy_procfs *pfs)
{
   return container_of(pfs, struct gpiomem_dummy, proc);
}

static inline
struct gpiomem_dummy *gd_get_from_vma(struct vm_area_struct *vma)
{
   return vma->vm_private_data;
}

#endif /* GPIOMEM_DUMMY_H_GUARD */
