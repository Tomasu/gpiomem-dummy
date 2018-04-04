#include "gpiomem_dummy_mmap.h"

#include <linux/mm.h>

#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "mmap: "

/* memory handler functions */

static void mmap_open(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_open\n");
   vma->vm_flags = (vma->vm_flags | VM_MAYREAD) & ~(VM_MAYWRITE /*| VM_WRITE*/);
}

static void mmap_close(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_close\n");
}

static int mmap_fault(struct vm_fault* vmf)
{
   struct page *page = NULL;
   struct vm_area_struct *vma = vmf->vma;

   printk(KERN_DEBUG LOG_PREFIX "fault: pgoff=0x%lx addr=0x%lx\n", vmf->pgoff, vmf->address - vma->vm_start);

   page = vmf->page;

   if(!page)
   {
      page = dummy_get()->page;
   }

   if(!page || IS_ERR(page))
   {
      printk(KERN_ERR LOG_PREFIX "unable to get mem pointer\n");
      return VM_FAULT_SIGBUS;
   }

   if(vmf->flags & VM_WRITE || vmf->flags & FAULT_FLAG_WRITE)
   {
      printk(KERN_DEBUG LOG_PREFIX "write to page!\n");
   }

   /*if(vmf->pgoff != BCM283X_GPIO_PGOFF)
   {
      printk(KERN_ERR LOG_PREFIX "attempt to access outside the gpio registers: pgoff:%lu gpio-pgoff:%lu", vmf->pgoff, BCM283X_GPIO_PGOFF);
      return VM_FAULT_SIGBUS;
   }*/

   printk(KERN_DEBUG LOG_PREFIX "address=0x%lx\n", vmf->address - vma->vm_start);

   vma->vm_flags = (vma->vm_flags | VM_MAYREAD) & ~(VM_MAYWRITE /*| VM_WRITE*/);

   vmf->page = page;
   get_page(page);

   return 0;
}

struct vm_operations_struct gpiomem_dummy_mmap_vmops = {
   .open  = mmap_open,  /* mmap-open */
   .close = mmap_close, /* mmap-close */
   .fault = mmap_fault, /* fault handler */
};
