/**
 * @file   gpiomem-dummy.c
 * @author Thomas Fjellstrom
 * @date   Mar 31 2018
 * @version 0.1
 * @brief   A dummy raspberry pi /dev/gpiomem device
 * @see based on http://www.derekmolloy.ie/ gpiomem device driver.
 */
#include <linux/version.h>
#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>          // Required for the copy to user function
#include <asm/setup.h>
#include <asm/page.h>
#include <linux/slab.h> // required for kzalloc
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#ifdef MODVERSIONS
#  include <linux/modversions.h>
#endif
#define  DEVICE_NAME "gpiomem"    ///< The device will appear at /dev/gpiomem using this value
#define  CLASS_NAME  "gpiomem"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Thomas Fjellstrom");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A dummy raspberry-pi gpiomem driver");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

#define GPIO_MEM_SIZE PAGE_SIZE

#define RPI_IO_MEM_START 0x3e000000Lu
#define RPI_IO_MEM_END 0x3effffffLu
#define RPI_IO_MEM_SIZE (RPI_IO_MEM_END - RPI_IO_MEM_START + 1)

#define RPI_GPIO_MEM_OFFSET 0x200000
#define RPI_GPIO_MEM_START (RPI_IO_MEM_START + RPI_GPIO_MEM_OFFSET)
#define RPI_GPIO_MEM_END (RPI_GPIO_MEM_START + GPIO_MEM_SIZE)
#define RPI_GPIO_MEM_PGOFF (RPI_GPIO_MEM_OFFSET >> PAGE_SHIFT)

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  gpiomemClass  = NULL; ///< The device-driver class struct pointer
static struct device* gpiomemDevice = NULL; ///< The device-driver device struct pointer

static int *kmalloc_area = NULL;
static int *kmalloc_ptr = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static int dev_mmap(struct file *file, struct vm_area_struct *vma);


/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .owner = THIS_MODULE,
   .open = dev_open,
   .release = dev_release,
   .mmap = dev_mmap,
};

/* memory handler functions */

static void dev_mmap_open(struct vm_area_struct *vma);
static void dev_mmap_close(struct vm_area_struct *vma);
static int dev_mmap_fault(struct vm_fault *vmf);

static struct vm_operations_struct dev_mem_ops = {
   .open  = dev_mmap_open,  /* mmap-open */
   .close = dev_mmap_close, /* mmap-close */
   .fault = dev_mmap_fault, /* fault handler */
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init gpiomem_init(void)
{
   int error_ret = 0;

   printk(KERN_INFO "gpiomem-dummy: Initializing the gpiomem-dummy LKM\n");

   kmalloc_ptr = kzalloc(GPIO_MEM_SIZE, GFP_KERNEL);
   if(!kmalloc_ptr) {
      printk(KERN_ALERT "gpiomem-dummy failed to allocate memory\n");
      return -ENOMEM;
   }

   kmalloc_area = kmalloc_ptr;

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "gpiomem-dummy failed to register a major number\n");
      return majorNumber;
   }

   printk(KERN_INFO "gpiomem-dummy: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   gpiomemClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(gpiomemClass))
   {
      printk(KERN_ALERT "Failed to register device class\n");

      error_ret = PTR_ERR(gpiomemClass);          // Correct way to return an error on a pointer
      goto dev_cleanup;
   }

   printk(KERN_INFO "gpiomem-dummy: device class registered correctly\n");

   // Register the device driver
   gpiomemDevice = device_create(gpiomemClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(gpiomemDevice))
   {
      printk(KERN_ALERT "Failed to create the device\n");

      error_ret = PTR_ERR(gpiomemDevice);
      goto dev_cleanup;
   }

   printk(KERN_INFO "gpiomem-dummy: device class created correctly\n"); // Made it! device was initialized
   return 0;

dev_cleanup:
   if(gpiomemClass)
   {
      class_destroy(gpiomemClass);
      gpiomemClass = NULL;
   }

   unregister_chrdev(majorNumber, DEVICE_NAME);

   return error_ret;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit gpiomem_exit(void)
{
   device_destroy(gpiomemClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(gpiomemClass);                          // unregister the device class
   class_destroy(gpiomemClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   kfree(kmalloc_ptr);                                      // free our memory

   printk(KERN_INFO "gpiomem-dummy: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
   numberOpens++;
   printk(KERN_INFO "gpiomem-dummy: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

static int dev_mmap(struct file* file, struct vm_area_struct* vma)
{
   unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
   unsigned long size = vma->vm_end - vma->vm_start;

   if (offset & ~PAGE_MASK)
   {
      printk(KERN_ALERT "gpiomem-dummy: offset not aligned: %ld\n", offset);
      return -ENXIO;
   }

   if (size > RPI_IO_MEM_SIZE)
   {
      printk(KERN_ALERT "gpiomem-dummy: size too big: %lu > %lu\n", size, RPI_IO_MEM_SIZE);
      return -ENXIO;
   }

   if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
   {
      printk(KERN_ALERT "gpiomem-dummy: writeable mappings must be shared, rejecting\n");
      return(-EINVAL);
   }

   // TODO: check if this is required
   // locks vma in ram, wont be swapped out
   vma->vm_flags |= VM_LOCKED;

   vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

   vma->vm_ops = &dev_mem_ops;

   dev_mmap_open(vma);

   /*
   if(remap_pfn_range(vma, vma->vm_start, virt_to_phys(kmalloc_area), size, vma->vm_page_prot))
   {
      printk(KERN_ALERT "gpiomem-dummy: remap page range failed\n");
      return -ENXIO;
}+ RPI_
   */

   return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
   printk(KERN_INFO "gpiomem-dummy: Device successfully closed\n");
   return 0;
}

void dev_mmap_open(struct vm_area_struct* vma)
{
}

void dev_mmap_close(struct vm_area_struct* vma)
{
}

int dev_mmap_fault(struct vm_fault* vmf)
{
   struct page *page = NULL;


   if(vmf->pgoff != RPI_GPIO_MEM_PGOFF)
   {
      printk(KERN_ERR "gpiomem-dummy: attempt to access outside the gpio registers\n");
      return VM_FAULT_SIGBUS;
   }

   page = virt_to_page(kmalloc_area);
   get_page(page); // inc refcount to page

   vmf->page = page;

   return 0;
}



/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(gpiomem_init);
module_exit(gpiomem_exit);
