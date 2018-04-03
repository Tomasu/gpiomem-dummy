#include "gpiomem_dummy_mmap.h"

#include <linux/mm.h>

#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "mmap: "

/* memory handler functions */

static void mmap_open(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_open\n");
}

static void mmap_close(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_close\n");
}

static int mmap_fault(struct vm_fault* vmf)
{
   void *ptr = NULL;
   struct page *page = NULL;

   ptr = dummy_get_mem_ptr();
   if(IS_ERR(ptr))
   {
      printk(KERN_ERR LOG_PREFIX "unable to get mem pointer\n");
      return VM_FAULT_SIGBUS;
   }

   if(vmf->pgoff != BCM283X_GPIO_PGOFF)
   {
      printk(KERN_ERR LOG_PREFIX "attempt to access outside the gpio registers: pgoff:%lu gpio-pgoff:%lu", vmf->pgoff, BCM283X_GPIO_PGOFF);
      return VM_FAULT_SIGBUS;
   }

   page = virt_to_page(ptr);
   get_page(page); // inc refcount to page

   vmf->page = page;

   return 0;
}

struct vm_operations_struct gpiomem_dummy_mmap_vmops = {
   .open  = mmap_open,  /* mmap-open */
   .close = mmap_close, /* mmap-close */
   .fault = mmap_fault, /* fault handler */
};
