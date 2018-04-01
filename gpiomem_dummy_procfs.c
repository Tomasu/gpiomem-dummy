#include "gpiomem_dummy_procfs.h"

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "gpiomem_dummy.h"

#define RANGES_SIZE 12
static char ranges_data[RANGES_SIZE];

static ssize_t proc_read(struct file *filp, char *buf, size_t count, loff_t *offp);

static struct file_operations proc_fops = {
   .read = proc_read // read from our device tree ranges file
};

int gpiomem_dummy_procfs_init(struct gpiomem_dummy_procfs *pfs)
{
   memset(ranges_data, 0, sizeof(ranges_data));

   pfs->proc_dt_ent = proc_mkdir("device-tree", NULL);
   if(PTR_ERR(pfs->proc_dt_ent))
   {
      printk(KERN_ERR LOG_PREFIX "failed to create device-tree dir\n");
      return -ENOMEM;
   }

   pfs->proc_soc_ent = proc_mkdir("soc", pfs->proc_dt_ent);
   if(PTR_ERR(pfs->proc_soc_ent))
   {
      printk(KERN_ERR LOG_PREFIX "failed to create device-tree/soc dir\n");
      proc_remove(pfs->proc_dt_ent);
      pfs->proc_dt_ent = NULL;
      return -ENOMEM;
   }

   pfs->proc_ranges_ent = proc_create("ranges", 0, pfs->proc_soc_ent, &proc_fops);
   if(PTR_ERR(pfs->proc_ranges_ent))
   {
      proc_remove(pfs->proc_soc_ent);
      pfs->proc_soc_ent = NULL;

      proc_remove(pfs->proc_dt_ent);
      pfs->proc_dt_ent = NULL;

      printk(KERN_ERR LOG_PREFIX "failed to create bcm device-tree procfs ranges entry\n");
      return -ENOMEM;
   }

   return 0;
}

void gpiomem_dummy_procfs_destroy(struct gpiomem_dummy_procfs *pfs)
{
   if(PTR_ERR(pfs))
   {
      printk(KERN_ERR LOG_PREFIX "null profs ptr\n");
      return;
   }

   if(PTR_ERR(pfs->proc_ranges_ent))
   {
      printk(KERN_ERR LOG_PREFIX "null procfs ranges entry\n");
      return;
   }

   proc_remove(pfs->proc_ranges_ent);
   pfs->proc_ranges_ent = NULL;

   if(PTR_ERR(pfs->proc_soc_ent))
   {
      printk(KERN_ERR LOG_PREFIX "null procfs soc entry\n");
      return;
   }

   proc_remove(pfs->proc_soc_ent);
   pfs->proc_soc_ent = NULL;

   if(PTR_ERR(pfs->proc_dt_ent))
   {
      printk(KERN_ERR LOG_PREFIX "null procfs device-tree entry\n");
      return;
   }

   proc_remove(pfs->proc_dt_ent);
   pfs->proc_dt_ent = NULL;
}

ssize_t proc_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
   ssize_t bytes_left = 0;
   ssize_t to_copy = 0;
   ssize_t ctu_ret = 0;

   if(*offp >= RANGES_SIZE)
   {
      return 0;
   }

   printk(KERN_INFO LOG_PREFIX "proc req read %ld at %lld\n", count, *offp);

   bytes_left = RANGES_SIZE - *offp;
   to_copy = min((ssize_t)count, bytes_left);

   printk(KERN_INFO LOG_PREFIX "proc real read %zd at %lld\n", to_copy, *offp);

   ctu_ret = copy_to_user(buf, ranges_data + *offp, to_copy);
   if(ctu_ret != to_copy)
   {
      printk(KERN_INFO LOG_PREFIX "copy_to_user didn't copy everything?? %zd of %zd\n", ctu_ret, to_copy);

      to_copy = ctu_ret;
   }

   *offp += to_copy;

   return to_copy;
}
