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
#  include <linux/modvresions.h>
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

struct gpiomem_dummy *gd_get()
{
   return dummy;
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
   unsigned long *pdata = NULL;

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

   /*
   error_ret = gd_pdrv_init(&new_dummy->pdev);
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

   pr_info("alloc page");
   new_dummy->page = alloc_page(GFP_USER | __GFP_ZERO);
   check_error(new_dummy->page, "failed to allocate page");

   pdata = (unsigned long*)page_to_virt(new_dummy->page);
   memset(pdata, 0, PAGE_SIZE);

   //pr_info("set_page_ro");
   //error_ret = gd_set_page_ro(new_dummy->page);
   //check_val_cleanup(error_ret, "failed to set page ro");

   error_ret = gd_cdev_init(&(new_dummy->cdev));
   if(error_ret != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to create char device\n");
      goto err_cleanup;
   }

   //new_dummy->page->mapping->a_ops = &cdev_aops;

   INIT_LIST_HEAD_RCU(&new_dummy->probe_list);

   new_dummy->initialized = 1;

   dummy = new_dummy;

   return 0;

err_cleanup:

   gd_cdev_destroy(&new_dummy->cdev);

   if(new_dummy->page)
   {
      __free_pages(new_dummy->page, 0);
      new_dummy->page = NULL;
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
   gd_cdev_destroy(&dummy->cdev);

   if(dummy->page)
   {
      __free_pages(dummy->page, 0);
      dummy->page = NULL;
   }

   dummy->initialized = 0;

   dummy = NULL;

   printk(KERN_INFO LOG_PREFIX "Goodbye from the LKM!\n");
}

static pte_t *get_pte(struct page *page)
{
   pgd_t *pgd;
   p4d_t *p4d;
   pud_t *pud;
   pmd_t *pmd;
   pte_t *pte;
   pte_t tmp_pte;

   unsigned long addr = 0;

   pr_info("check args");

   if(!page)
   {
      pr_err("invalid page");
      return NULL;
   }

   if(!current)
   {
      pr_err("no current task_struct");
      return NULL;
   }

   if(!current->mm)
   {
      pr_err("no mm struct");
      return NULL;
   }

   pr_info("page_to_virt");

   addr = (unsigned long)page_to_virt(page);
   check_error((void*)addr, "invalid pfn");

   pr_info("pgd_offset");
   pgd = pgd_offset(current->mm, addr);
   if (pgd_none(*pgd))
   {
      pgd_ERROR(*pgd);
      return NULL;
   }

   pr_info("p4d_offset");
   p4d = p4d_offset(pgd, addr);
   if (p4d_none(*p4d))
   {
      p4d_ERROR(*p4d);
      return NULL;
   }

   pr_info("pud_offset");
   pud = pud_offset(p4d, addr);
   if (pud_none(*pud))
   {
      pud_ERROR(*pud);
      return NULL;
   }

   pr_info("pmd_offset");
   pmd = pmd_offset(pud, addr);
   if (pmd_none(*pmd))
   {
      pmd_ERROR(*pmd);
      return NULL;
   }

   pr_info("pte_offset_map");
   pte = pte_offset_map(pmd, addr);
   if (pte_none(*pte))
   {
      pte_ERROR(*pte);
      return NULL;
   }

   return pte;
}

int gd_set_page_rw(struct page *page)
{
   pte_t *pte = get_pte(page);
   pte_t tmp_pte;

   if(!pte)
   {
      return -1;
   }

   tmp_pte = *pte;

   pr_info("set pte");
   set_pte(pte, pte_clear_flags(tmp_pte, _PAGE_RW | _PAGE_PRESENT));

   pr_info("ret");
   return 0;
}

int gd_set_page_ro(struct page *page)
{
   pte_t *pte = get_pte(page);
   pte_t tmp_pte;

   if(!pte)
   {
      return -1;
   }

   tmp_pte = *pte;

   pr_info("set pte");
   set_pte(pte, pte_clear_flags(tmp_pte, _PAGE_RW | _PAGE_PRESENT));

   //tmp_pte = *pte;
   //set_pte(pte, pte_set_flags(tmp_pte, _PAGE_PROTNONE | _PAGE_ACCESSED));

   pr_info("ret");
   return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(gpiomem_init);
module_exit(gpiomem_exit);
