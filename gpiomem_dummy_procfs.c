#include "gpiomem_dummy_procfs.h"

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "gpiomem_dummy.h"

#define RANGES_DATA_NUM 3
#define RANGES_SIZE (RANGES_DATA_NUM * sizeof(*ranges_data))
static u32 ranges_data[RANGES_DATA_NUM] = { 0, BCM283X_PERIPH_BASE, BCM283X_PERIPH_SIZE };

static ssize_t proc_read(struct file *filp, char *buf, size_t count, loff_t *offp);
static loff_t proc_llseek(struct file *filp, loff_t offset, int whence);

static struct file_operations proc_fops = {
   .owner = THIS_MODULE, // WE OWN YOU
   .read = proc_read, // read from our device tree ranges file
   .llseek = proc_llseek // seek!
};

int gpiomem_dummy_procfs_init(struct gpiomem_dummy_procfs *pfs)
{
   int error_ret = 0;
   struct proc_dir_entry *dt_ent = NULL;
   struct proc_dir_entry *soc_ent = NULL;
   struct proc_dir_entry *ranges_ent = NULL;

   memset(pfs, 0, sizeof(*pfs));

   dt_ent = proc_mkdir("device-tree", NULL);
   if(IS_ERR(dt_ent))
   {
      printk(KERN_ALERT LOG_PREFIX "failed to create device-tree dir\n");
      return PTR_ERR(dt_ent);
   }

   soc_ent = proc_mkdir("soc", dt_ent);
   if(IS_ERR(soc_ent))
   {
      printk(KERN_ALERT LOG_PREFIX "failed to create device-tree/soc dir\n");

      proc_remove(dt_ent);
      error_ret = PTR_ERR(soc_ent);
      goto error_cleanup;
   }

   ranges_ent = proc_create("ranges", 0, soc_ent, &proc_fops);
   if(IS_ERR(ranges_ent))
   {
      printk(KERN_ALERT LOG_PREFIX "failed to create bcm device-tree procfs ranges entry\n");

      proc_remove(soc_ent);
      proc_remove(dt_ent);

      error_ret = PTR_ERR(ranges_ent);
      goto error_cleanup;
   }

   pfs->proc_dt_ent = dt_ent;
   pfs->proc_soc_ent = soc_ent;
   pfs->proc_ranges_ent = ranges_ent;

   return 0;

error_cleanup:
   if(!IS_ERR(soc_ent))
   {
      proc_remove(soc_ent);
   }

   if(!IS_ERR(dt_ent))
   {
      proc_remove(dt_ent);
   }

   return error_ret;
}

void gpiomem_dummy_procfs_destroy(struct gpiomem_dummy_procfs *pfs)
{
   if(!pfs)
   {
      printk(KERN_ERR LOG_PREFIX "Null pfs?!\n");
      return;
   }

   if(pfs->proc_ranges_ent)
   {
      proc_remove(pfs->proc_ranges_ent);
      pfs->proc_ranges_ent = NULL;
   }

   if(pfs->proc_soc_ent)
   {
      proc_remove(pfs->proc_soc_ent);
      pfs->proc_soc_ent = NULL;
   }

   if(pfs->proc_dt_ent)
   {
      proc_remove(pfs->proc_dt_ent);
      pfs->proc_dt_ent = NULL;
   }
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
   if(ctu_ret != 0)
   {
      printk(KERN_INFO LOG_PREFIX "copy_to_user didn't copy everything?? %zd of %zd\n", to_copy-ctu_ret, to_copy);

      to_copy = ctu_ret;
   }

   *offp += to_copy;

   return to_copy;
}

#define my_min(a, b) ({ \
   typeof ((a)) a_copy_ = (a); \
   typeof ((b)) b_copy_ = (b); \
   a_copy_ > b_copy_ ? b_copy_ : a_copy_; \
})

#define my_max(a, b) ({ \
   typeof ((a)) a_copy_ = (a); \
   typeof ((b)) b_copy_ = (b); \
   a_copy_ < b_copy_ ? b_copy_ : a_copy_; \
})

loff_t proc_llseek(struct file *filp, loff_t offset, int whence)
{
   loff_t new_pos = 0;

   switch(whence)
   {
      case SEEK_SET:
         new_pos = offset;
         break;

      case SEEK_CUR:
         new_pos = filp->f_pos + offset;
         break;

      case SEEK_END:
         new_pos = RANGES_SIZE - 1;
         break;

      default:
         printk(KERN_ERR LOG_PREFIX "invalid seek whence >:(\n");
         new_pos = filp->f_pos;
         break;
   }

   new_pos = my_max(0, my_min(new_pos, RANGES_SIZE));
   filp->f_pos = new_pos;

   return new_pos;
}
