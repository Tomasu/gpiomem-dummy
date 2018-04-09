#include "gpiomem_dummy_cdev.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm-generic/pgtable.h>

#include "gpiomem_dummy_log.h"

#include "gpiomem_dummy.h"
#include "gpiomem_dummy_mmap.h"
#include <linux/mman.h>

static int gd_cdev_open(struct inode *inodep, struct file *filep);
static int gd_cdev_mmap(struct file* file, struct vm_area_struct* vma);
static int gd_cdev_release(struct inode *inodep, struct file *filep);

static ssize_t gd_cdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gd_cdev_write(struct file *, const char __user *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations gd_cdev_fops =
{
   .owner = THIS_MODULE,
   .open = gd_cdev_open,
   .release = gd_cdev_release,
   .mmap = gd_cdev_mmap,
   .llseek = no_llseek,
   .read = gd_cdev_read,
   .write = gd_cdev_write,
};

int gd_cdev_init(struct gpiomem_dummy_cdev *cdev)
{
   int cdev_major = -1;
   struct cdev *cdevp = NULL;
   struct class *clss = NULL;
   struct device *dev = NULL;
   int dev_id = -1;
   dev_t devt;
   int error_ret = -ENODEV;

   pr_info("init");
   check_error(cdev, "Null cdev?");

   memset(cdev, 0, sizeof(*cdev));

   pr_info("register chrdev");
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   error_ret = alloc_chrdev_region(&devt, 0, 1, DEVICE_NAME);
   check_val_cleanup(error_ret, "failed to register chrdev region");
   cdev_major = MAJOR(devt);

   pr_info("registered major %d", cdev_major);

   // Register the device driver
   dev_id = MKDEV(cdev_major, 0);

   // register our class/device so the kernel will auto create /dev entries
   clss = class_create(THIS_MODULE, CLASS_NAME);
   check_error_cleanup(clss, "failed to create class");

   pr_info("registered device class");

   dev = device_create(clss, NULL, dev_id, cdev, DEVICE_NAME);
   check_error_cleanup(dev, "failed to create device");

   cdevp = &cdev->cdev;
   cdev_init(cdevp, &gd_cdev_fops);
   cdevp->owner = THIS_MODULE;
   error_ret = cdev_add(cdevp, dev_id, 1);
   check_val_cleanup(error_ret, "failed to add chardev");

   pr_info("created chardev");

   cdev->major_number = cdev_major;
   cdev->dev_id = dev_id;
   cdev->clss = clss;
   cdev->dev = dev;

   pr_info("device created");
   return 0;

err_cleanup:
   if(cdevp && !IS_ERR(cdevp))
   {
      cdev_del(cdevp);
   }

   if (clss && !IS_ERR(clss))
   {
      if (dev && !IS_ERR(dev))
      {
         device_destroy(clss, dev_id);
      }

      class_destroy(clss);
   }

   if(cdev_major)
   {
      unregister_chrdev(cdev_major, DEVICE_NAME);
   }


   return error_ret;
}

void gd_cdev_destroy(struct gpiomem_dummy_cdev *cdev)
{
   cdev_del(&cdev->cdev);

   if (cdev->clss && !IS_ERR(cdev->clss))
   {
      if(cdev->dev && !IS_ERR(cdev->dev))
      {
         device_destroy(cdev->clss, cdev->dev_id);
         cdev->dev_id = 0;
         cdev->dev = NULL;
      }

      class_destroy(cdev->clss); // remove the device class
      cdev->clss = NULL;
   }

   if(cdev->major_number)
   {
      unregister_chrdev_region(cdev->dev_id, 1); // unregister the major number
      cdev->dev_id = 0;
      cdev->major_number = 0;
   }
}

inline static struct gpiomem_dummy_cdev *inode_to_cdev(struct inode *ip)
{
   return container_of(ip->i_cdev, struct gpiomem_dummy_cdev, cdev);
}

inline static struct gpiomem_dummy *file_to_gd(struct file *fp)
{
   return fp->private_data;
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int gd_cdev_open(struct inode *inodep, struct file *filep)
{
   struct gpiomem_dummy_cdev *cdev = inode_to_cdev(inodep);
   if(!cdev)
   {
      pr_err("cdev not found?!");
      return -ENODEV;
   }

   filep->private_data = gd_cdev_get_dummy(cdev);

   cdev->times_opened++;

   pr_info("device has been opened %d time(s)", cdev->times_opened);

   return nonseekable_open(inodep, filep);
}

static int gd_cdev_mmap(struct file* fp, struct vm_area_struct* vma)
{
   unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
   unsigned long size = vma->vm_end - vma->vm_start;
   struct gpiomem_dummy *gd = NULL;

   pr_info("mmap request!");

   if (offset & ~PAGE_MASK)
   {
      pr_err("offset not aligned: %ld", offset);
      return -ENXIO;
   }

   /*if (vma->vm_pgoff != BCM283X_PERIPH_PGOFF)
   {
      pr_err("attempt to map outside of peripheral memory: vpgo=%d pbgo=%d", vma->vm_pgoff, BCM283X_PERIPH_PGOFF);
      return -ENXIO;
   }*/

   if (size > BCM283X_PERIPH_SIZE)
   {
      pr_err("size too big: %lu > %lu", size, BCM283X_PERIPH_SIZE);
      return -ENXIO;
   }

   if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
   {
      pr_err("writable mappings must be shared, rejecting");
      return(-EINVAL);
   }

   gd = file_to_gd(fp);
   if (!gd)
   {
      pr_err("failed to get cdev!?");
      return -ENODEV;
   }

   vma->vm_private_data = gd;

   vma->vm_ops = &gpiomem_dummy_mmap_vmops;

   vma->vm_ops->open(vma);

   pr_info("mmap success!");

   // gd_cdev_mmap_open(vma); <-- is it really necessary to call the mmap open function here if we have one set up?? I'd think it's already been called....

   return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int gd_cdev_release(struct inode *inodep, struct file *filep)
{
   pr_info("Device successfully closed");
   return 0;
}

static ssize_t gd_cdev_read(struct file *filp, char __user *data, size_t len, loff_t *offset)
{
   pr_info("read!");

   return len;
}

static ssize_t gd_cdev_write(struct file *filp, const char __user *data, size_t len, loff_t *offset)
{
   pr_info("write!");
   return len;
}
