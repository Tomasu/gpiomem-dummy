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
#include <linux/platform_device.h>

#ifdef MODVERSIONS
#  include <linux/modversions.h>
#endif

#include "gpiomem_dummy_procfs.h"
#include "gpiomem_dummy_cdev.h"
#include "gpiomem_dummy_pdev.h"

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Thomas Fjellstrom");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A dummy raspberry-pi gpiomem driver");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

#define LOG_PREFIX LOG_PREFIX_ "mod: "

static struct gpiomem_dummy *dummy = NULL;

struct gpiomem_dummy *dummy_get()
{
   if(!dummy || !dummy->initialized)
   {
      printk(KERN_ERR LOG_PREFIX "driver not initialized\n");
      return NULL;
   }

   return dummy;
}

void *dummy_get_mem_ptr()
{
   if(!dummy || !dummy->initialized)
   {
      return NULL;
   }

   return dummy->kmalloc_ptr;
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
   struct gpiomem_dummy *new_dummy = NULL;

   if(dummy)
   {
      printk(KERN_ERR LOG_PREFIX "already initialized!?\n");
      return -EBUSY;
   }

   new_dummy = kcalloc(1, sizeof(*new_dummy), GFP_KERNEL);
   if(!new_dummy)
   {
      printk(KERN_ERR LOG_PREFIX "failed to allocate dummy struct\n");
      return -ENOMEM;
   }

   /*error_ret = gpiomem_dummy_pdrv_init(&new_dummy->pdev);
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to initialize pdev\n");
      return error_ret;
   }*/

   error_ret = gpiomem_dummy_procfs_init(&(new_dummy->proc));
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to create procfs entry\n");
      return error_ret;
   }

   printk(KERN_INFO LOG_PREFIX "Initializing the gpiomem-dummy LKM\n");

   new_dummy->page = alloc_page(GFP_KERNEL);

   new_dummy->kmalloc_ptr = kzalloc(KALLOC_MEM_SIZE, GFP_KERNEL);
   if(!new_dummy->kmalloc_ptr)
   {
      printk(KERN_ALERT LOG_PREFIX "failed to allocate memory\n");

      error_ret = -ENOMEM;
      goto err_cleanup;
   }

   new_dummy->kmalloc_area = new_dummy->kmalloc_ptr;

   error_ret = gpiomem_dummy_cdev_init(&(new_dummy->cdev));
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to create char device\n");
      goto err_cleanup;
   }

   new_dummy->page->mapping->a_ops = &cdev_aops;

   new_dummy->initialized = 1;

   dummy = new_dummy;

   return 0;

err_cleanup:

   gpiomem_dummy_cdev_destroy(&new_dummy->cdev);

   if(new_dummy->page)
   {
      __free_pages(new_dummy->page, 0);
      new_dummy->page = NULL;
   }

   if(new_dummy->kmalloc_ptr)
   {
      kfree(new_dummy->kmalloc_ptr);
      new_dummy->kmalloc_ptr = NULL;
      new_dummy->kmalloc_area = NULL;
   }

   gpiomem_dummy_procfs_destroy(&new_dummy->proc);

   //gpiomem_dummy_pdrv_exit(&new_dummy->pdev);

   new_dummy->initialized = 0;

   kfree(new_dummy);

   return error_ret;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit gpiomem_exit(void)
{
   if(!dummy)
   {
      printk(KERN_ERR LOG_PREFIX "not initialized, can't deinitialize\n");
      return;
   }

   gpiomem_dummy_procfs_destroy(&dummy->proc);
   gpiomem_dummy_cdev_destroy(&dummy->cdev);

   if(dummy->kmalloc_ptr)
   {
      kfree(dummy->kmalloc_ptr); // free our memory
      dummy->kmalloc_ptr = NULL;
      dummy->kmalloc_area = NULL;
   }

   if(dummy->page)
   {
      __free_pages(dummy->page, 0);
      dummy->page = NULL;
   }

   dummy->initialized = 0;

   dummy = NULL;

   printk(KERN_INFO LOG_PREFIX "Goodbye from the LKM!\n");
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(gpiomem_init);
module_exit(gpiomem_exit);
