#include "gpiomem_dummy_procfs.h"

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "gpiomem_dummy.h"

#define RANGES_SIZE 12

#define RANGES_DATA(data, idx, val) data[idx*4] = (u8)(val << 24); data[idx*4+1] = (u8)(val << 16); data[idx*4+2] = (u8)(val << 8); data[idx*4+3] = (u8)val

static ssize_t proc_read(struct file *filp, char *buf, size_t count, loff_t *offp);
static loff_t proc_llseek(struct file *filp, loff_t offset, int whence);

static struct file_operations proc_fops = {
   .owner = THIS_MODULE, // WE OWN YOU
   .read = proc_read, // read from our device tree ranges file
   .llseek = proc_llseek // seek!
};

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

int gpiomem_dummy_procfs_init(struct gpiomem_dummy_procfs *pfs)
{
   int error_ret = 0;
   struct proc_dir_entry *dt_ent = NULL;
   struct proc_dir_entry *soc_ent = NULL;
   struct proc_dir_entry *ranges_ent = NULL;

   if(!pfs)
   {
      printk(KERN_ALERT LOG_PREFIX "null pfs?!\n");
      return -EFAULT;
   }

   memset(pfs, 0, sizeof(*pfs));

   pfs->mark_me = 0xDEADBEEF;

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

   ranges_ent = proc_create_data("ranges", 0, soc_ent, &proc_fops, pfs);
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

   RANGES_DATA(pfs->ranges_data, 0, 0);
   RANGES_DATA(pfs->ranges_data, 1, BCM283X_PERIPH_BASE);
   RANGES_DATA(pfs->ranges_data, 2, BCM283X_PERIPH_SIZE);

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
   ssize_t to_copy = count;
   ssize_t ctu_ret = 0;
   int i = 0;
   int num_ints = 0;
   struct gpiomem_dummy_procfs *pfs = NULL;

   if(*offp >= RANGES_SIZE)
   {
      printk(KERN_INFO LOG_PREFIX "proc req read %lld >= RANGES_SIZE %u\n", *offp, RANGES_SIZE);
      return 0;
   }

   pfs = PDE_DATA(filp->f_inode);
   if(!pfs)
   {
      printk(KERN_ERR LOG_PREFIX "Failed to find pde data in file handle\n");
      return -ENOENT;
   }

   if(pfs->mark_me != 0xDEADBEEF)
   {
      printk(KERN_INFO LOG_PREFIX "pde data seems to be wrong\n");
      return -ENOENT;
   }

   printk(KERN_INFO LOG_PREFIX "proc req read num=%ld at off=%lld\n", count, *offp);

   if(*offp + count > RANGES_SIZE)
      to_copy = RANGES_SIZE - *offp;

   printk(KERN_INFO LOG_PREFIX "proc real read num=%zd at off=%lld\n", to_copy, *offp);

   num_ints = to_copy / 4;
   for(i = *offp / 4; i < num_ints+1; i++)
   {
      u32 d = ((u32*)pfs->ranges_data)[i];
      printk(KERN_DEBUG LOG_PREFIX "proc read idx=%d numi=%d val=%x (%d,%d,%d,%d)\n", i, num_ints, d, pfs->ranges_data[i], pfs->ranges_data[i+1], pfs->ranges_data[i+2], pfs->ranges_data[i+3]);
   }

   ctu_ret = copy_to_user(buf, pfs->ranges_data + *offp, to_copy);
   if(ctu_ret != 0)
   {
      printk(KERN_INFO LOG_PREFIX "copy_to_user didn't copy everything?? %zd of %zd\n", to_copy-ctu_ret, to_copy);

      to_copy += ctu_ret;
   }

   *offp += to_copy;

   return to_copy;
}

const char *whence_str[] = {
   "SEEK_SET", "SEEK_CUR", "SEEK_END", NULL
};

loff_t proc_llseek(struct file *filp, loff_t offset, int whence)
{
   return fixed_size_llseek(filp, offset, whence, RANGES_SIZE);
}
