/**
 * @file   gpiomem-dummy.c
 * @author Thomas Fjellstrom
 * @date   Mar 31 2018
 * @version 0.1
 * @brief   A dummy raspberry pi /dev/gpiomem device
 * @see based on http://www.derekmolloy.ie/ gpiomem device driver.
 */

#include "gpiomem_dummy.h"

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
#include <linux/err.h>

#ifdef MODVERSIONS
#  include <linux/modversions.h>
#endif

#include "gpiomem_dummy_procfs.h"
#include "gpiomem_dummy_cdev.h"

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Thomas Fjellstrom");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A dummy raspberry-pi gpiomem driver");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static struct gpiomem_dummy dummy =
{
   .initialized = 0
};

struct gpiomem_dummy *dummy_get()
{
   if(!dummy.initialized)
   {
      printk(KERN_ERR LOG_PREFIX "driver not initialized\n");
      return NULL;
   }

   return &dummy;
}

void *dummy_get_mem_ptr()
{
   if(!dummy.initialized)
   {
      return NULL;
   }

   return dummy.kmalloc_ptr;
}

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init gpiomem_init(void)
{
   int error_ret = 0;

   if(dummy.initialized)
   {
      printk(KERN_ERR LOG_PREFIX "already initialized!?\n");
      return -EBUSY;
   }

   error_ret = gpiomem_dummy_procfs_init(&dummy.proc);
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to create procfs entry\n");
      return error_ret;
   }

   printk(KERN_INFO LOG_PREFIX "Initializing the gpiomem-dummy LKM\n");

   dummy.kmalloc_ptr = kzalloc(KALLOC_MEM_SIZE, GFP_KERNEL);
   if(!dummy.kmalloc_ptr)
   {
      printk(KERN_ALERT LOG_PREFIX "failed to allocate memory\n");

      error_ret = -ENOMEM;
      goto err_cleanup;
   }

   dummy.kmalloc_area = dummy.kmalloc_ptr;

   error_ret = gpiomem_dummy_cdev_init(&dummy.cdev);
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to create char device\n");
      goto err_cleanup;
   }

   dummy.initialized = 1;

   return 0;

err_cleanup:
   if(dummy.cdev.major_number)
   {
      gpiomem_dummy_cdev_destroy(&dummy.cdev);
   }

   if(dummy.kmalloc_ptr)
   {
      kfree(dummy.kmalloc_ptr);
      dummy.kmalloc_ptr = NULL;
      dummy.kmalloc_area = NULL;
   }

   gpiomem_dummy_procfs_destroy(&dummy.proc);

   dummy.initialized = 0;

   return error_ret;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit gpiomem_exit(void)
{
   gpiomem_dummy_procfs_destroy(&dummy.proc);
   gpiomem_dummy_cdev_destroy(&dummy.cdev);

   kfree(dummy.kmalloc_ptr); // free our memory
   dummy.kmalloc_ptr = NULL;
   dummy.kmalloc_area = NULL;

   dummy.initialized = 0;

   printk(KERN_INFO LOG_PREFIX "Goodbye from the LKM!\n");
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(gpiomem_init);
module_exit(gpiomem_exit);
