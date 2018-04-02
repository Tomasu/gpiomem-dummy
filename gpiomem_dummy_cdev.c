#include "gpiomem_dummy_cdev.h"

#include <linux/version.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/errno.h>

#include "gpiomem_dummy.h"
#include "gpiomem_dummy_mmap.h"

static int dev_open(struct inode *inodep, struct file *filep);
static int dev_mmap(struct file* file, struct vm_area_struct* vma);
static int dev_release(struct inode *inodep, struct file *filep);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations cdev_fops =
{
   .owner = THIS_MODULE,
   .open = dev_open,
   .release = dev_release,
   .mmap = dev_mmap,
};

int gpiomem_dummy_cdev_init(struct gpiomem_dummy_cdev *cdev)
{
   int error_ret = -1;
   int chrdev_major = 0;

   if(!cdev)
   {
      printk(KERN_ERR LOG_PREFIX "Null cdev?!\n");
      return -ENODEV;
   }

   memset(cdev, 0, sizeof(*cdev));

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   chrdev_major = register_chrdev(0, DEVICE_NAME, &cdev_fops);
   if (chrdev_major < 0){
      printk(KERN_ALERT LOG_PREFIX "failed to register a major number\n");
      return chrdev_major;
   }

   cdev->major_number = chrdev_major;

   printk(KERN_INFO LOG_PREFIX "registered correctly with major number %d\n", cdev->major_number);

   // Register the device class
   cdev->clss = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(cdev->clss))
   {
      printk(KERN_ALERT LOG_PREFIX "Failed to register device class\n");

      error_ret = PTR_ERR(cdev->clss);          // Correct way to return an error on a pointer
      goto dev_cleanup;
   }

   printk(KERN_INFO LOG_PREFIX "device class registered correctly\n");

   // Register the device driver
   cdev->dev_id = MKDEV(cdev->major_number, 0);

   cdev->dev = device_create(cdev->clss, NULL, cdev->dev_id, NULL, DEVICE_NAME);
   if (IS_ERR(cdev->dev))
   {
      printk(KERN_ALERT LOG_PREFIX "Failed to create the device\n");

      error_ret = PTR_ERR(cdev->dev);
      goto dev_cleanup;
   }

   printk(KERN_INFO LOG_PREFIX "device class created correctly\n"); // Made it! device was initialized
   return 0;

dev_cleanup:
   if(!IS_ERR(cdev->clss))
   {
      class_destroy(cdev->clss);
      cdev->clss = NULL;
   }

   cdev->dev = NULL;

   if(cdev->major_number)
   {
      unregister_chrdev(cdev->major_number, DEVICE_NAME);
      cdev->major_number = 0;
   }

   return error_ret;
}

void gpiomem_dummy_cdev_destroy(struct gpiomem_dummy_cdev *cdev)
{
   if(cdev->dev)
   {
      device_destroy(cdev->clss, cdev->dev_id);
      cdev->dev_id = 0;
      cdev->dev = NULL;
   }

   if(cdev->clss)
   {
      class_unregister(cdev->clss);                          // unregister the device class
      class_destroy(cdev->clss);                             // remove the device class
      cdev->clss= NULL;
   }

   if(cdev->major_number)
   {
      unregister_chrdev(cdev->major_number, DEVICE_NAME);             // unregister the major number
      cdev->major_number = 0;
   }
}


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
   struct gpiomem_dummy *dummy = dummy_get();
   if(!dummy)
   {
      printk(KERN_ERR LOG_PREFIX "driver not initialized?!\n");
      return -ENODEV;
   }

   dummy->cdev.times_opened++;

   printk(KERN_INFO LOG_PREFIX "Device has been opened %d time(s)\n", dummy->cdev.times_opened);
   return 0;
}

static int dev_mmap(struct file* file __attribute__((unused)), struct vm_area_struct* vma)
{
   unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
   unsigned long size = vma->vm_end - vma->vm_start;

   printk(KERN_INFO LOG_PREFIX "mmap request!\n");

   if (offset & ~PAGE_MASK)
   {
      printk(KERN_ALERT LOG_PREFIX "offset not aligned: %ld\n", offset);
      return -ENXIO;
   }

   if (vma->vm_pgoff != BCM283X_PERIPH_PGOFF)
   {
      printk(KERN_ERR LOG_PREFIX "attempt to map outside of peripheral memory\n");
      return -ENXIO;
   }

   if (size > BCM283X_PERIPH_SIZE)
   {
      printk(KERN_ALERT LOG_PREFIX "size too big: %lu > %lu\n", size, BCM283X_PERIPH_SIZE);
      return -ENXIO;
   }

   if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
   {
      printk(KERN_ALERT LOG_PREFIX "writeable mappings must be shared, rejecting\n");
      return(-EINVAL);
   }

   // TODO: check if this is required
   // locks vma in ram, wont be swapped out
   vma->vm_flags |= VM_LOCKED;

   vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

   vma->vm_ops = &gpiomem_dummy_mmap_vmops;

   // dev_mmap_open(vma); <-- is it really necessary to call the mmap open function here if we have one set up?? I'd think it's already been called....

   return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
   printk(KERN_INFO LOG_PREFIX "Device successfully closed\n");
   return 0;
}


