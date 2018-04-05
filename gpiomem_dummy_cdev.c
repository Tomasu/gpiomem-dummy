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
#include <linux/mman.h>

#define LOG_PREFIX LOG_PREFIX_ "cdev: "

static int dev_open(struct inode *inodep, struct file *filep);
static int dev_mmap(struct file* file, struct vm_area_struct* vma);
static int dev_release(struct inode *inodep, struct file *filep);

static int cdev_writepage(struct page *page, struct writeback_control *wbc);
static int cdev_readpage(struct file *, struct page *);

/* Write back some dirty pages from this mapping. */
static int cdev_writepages(struct address_space *, struct writeback_control *);

/* Set a page dirty.  Return true if this dirtied it */
static int cdev_set_page_dirty(struct page *page);

static int cdev_readpages(struct file *filp, struct address_space *mapping,
                   struct list_head *pages, unsigned nr_pages);

static int cdev_write_begin(struct file *, struct address_space *mapping,
                     loff_t pos, unsigned len, unsigned flags,
                     struct page **pagep, void **fsdata);
static int cdev_write_end(struct file *, struct address_space *mapping,
                   loff_t pos, unsigned len, unsigned copied,
                   struct page *page, void *fsdata);

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
   .llseek = no_llseek,
};

struct address_space_operations cdev_aops =
{
   .writepage = cdev_writepage,
   .readpage = cdev_readpage,
   .writepages = cdev_writepages,
   .set_page_dirty = cdev_set_page_dirty,
   .readpages = cdev_readpages,
   .write_begin = cdev_write_begin,
   .write_end = cdev_write_end
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

   return nonseekable_open(inodep, filep);
}

static int dev_mmap(struct file* file __attribute__((unused)), struct vm_area_struct* vma)
{
   unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
   unsigned long size = vma->vm_end - vma->vm_start;

   printk(KERN_INFO LOG_PREFIX "mmap request!\n");

   if (offset & ~PAGE_MASK)
   {
      printk(KERN_ERR LOG_PREFIX "offset not aligned: %ld\n", offset);
      return -ENXIO;
   }

   /*if (vma->vm_pgoff != BCM283X_PERIPH_PGOFF)
   {
      printk(KERN_ERR LOG_PREFIX "attempt to map outside of peripheral memory: vpgo=%d pbgo=%d\n", vma->vm_pgoff, BCM283X_PERIPH_PGOFF);
      return -ENXIO;
   }*/

   if (size > BCM283X_PERIPH_SIZE)
   {
      printk(KERN_ERR LOG_PREFIX "size too big: %lu > %lu\n", size, BCM283X_PERIPH_SIZE);
      return -ENXIO;
   }

   if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
   {
      printk(KERN_ERR LOG_PREFIX "writable mappings must be shared, rejecting\n");
      return(-EINVAL);
   }

   vma->vm_file->f_mapping->a_ops = &cdev_aops;

   vma->vm_ops = &gpiomem_dummy_mmap_vmops;

   vma->vm_ops->open(vma);

   printk(KERN_INFO LOG_PREFIX "mmap success!\n");

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

int cdev_writepage(struct page *page, struct writeback_control *wbc)
{
   printk(KERN_DEBUG LOG_PREFIX "writepage\n");
   return 0;
}

int cdev_readpage(struct file *fp, struct page *pg)
{
   printk(KERN_DEBUG LOG_PREFIX "readpage\n");
   return 0;
}

/* Write back some dirty pages from this mapping. */
int cdev_writepages(struct address_space *as, struct writeback_control *wbc)
{
   printk(KERN_DEBUG LOG_PREFIX "writepages\n");
   return 0;
}

//[31111.087184] BUG: unable to handle kernel NULL pointer dereference at 0000000000000010
//[31111.088182] IP: vmacache_find+0x23/0xa0

/* Set a page dirty.  Return true if this dirtied it */
int cdev_set_page_dirty(struct page *page)
{
   printk(KERN_DEBUG LOG_PREFIX "set_page_dirty\n");

   unmap_mapping_range(page->mapping, 0, PAGE_SIZE, 1);

   //page->mapping

   return 1;
}

int cdev_readpages(struct file *filp, struct address_space *mapping,
                 struct list_head *pages, unsigned nr_pages)
{
   printk(KERN_DEBUG LOG_PREFIX "readpages\n");
   return 0;
}

int cdev_write_begin(struct file *filp, struct address_space *mapping,
                   loff_t pos, unsigned len, unsigned flags,
                   struct page **pagep, void **fsdata)
{
   printk(KERN_DEBUG LOG_PREFIX "write_begin\n");
   return 0;
}

int cdev_write_end(struct file *filp, struct address_space *mapping,
                 loff_t pos, unsigned len, unsigned copied,
                 struct page *page, void *fsdata)
{
   printk(KERN_DEBUG LOG_PREFIX "write_end\n");
   return 0;
}
