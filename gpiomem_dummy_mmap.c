#include "gpiomem_dummy_mmap.h"

#include <linux/mm.h>
#include <linux/pagemap.h>
#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "mmap: "

/* memory handler functions */

static void mmap_open(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_open\n");

   //vma->vm_flags = (vma->vm_flags | VM_DONTEXPAND | VM_DONTCOPY | VM_DONTDUMP | VM_IO | VM_MAYREAD | VM_MIXEDMAP);
   vma->vm_flags |= VM_READ | VM_DONTCOPY | VM_DONTDUMP | VM_DONTEXPAND | /*VM_IO | */VM_MIXEDMAP | VM_WRITE;
   //vma->vm_flags &= ~(VM_MAYWRITE | VM_WRITE);
   vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
}

static void mmap_close(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_close\n");
}

static int clr_pte(struct vm_fault *vmf)
{
   return 0;
}

static int mmap_fault(struct vm_fault* vmf)
{
   struct page *page = NULL;
   struct vm_area_struct *vma = vmf->vma;
   unsigned long addr = 0;
   pte_t tmp_pte;
   pgd_t *pgd;
   p4d_t *p4d;
   pud_t *pud;
   pmd_t *pmd;
   pte_t *pte;

   printk(KERN_DEBUG LOG_PREFIX "fault: pgoff=0x%lx addr=0x%lx\n", vmf->pgoff, vmf->address - vma->vm_start);

   printk("fault: flags ");
   if(vmf->flags & FAULT_FLAG_INSTRUCTION)
      printk("INST ");
   if(vmf->flags & FAULT_FLAG_KILLABLE)
      printk("KILLABLE ");
   if(vmf->flags & FAULT_FLAG_MKWRITE)
      printk("MKWRITE ");
   if(vmf->flags & FAULT_FLAG_REMOTE)
      printk("REMOTE ");
   if(vmf->flags & FAULT_FLAG_TRIED)
      printk("TRIED ");
   if(vmf->flags & FAULT_FLAG_USER)
      printk("USER ");
   if(vmf->flags & FAULT_FLAG_WRITE)
      printk("WRITE ");

   printk("\n");

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

   if(vma->vm_flags & VM_WRITE)
   {
      printk(KERN_DEBUG LOG_PREFIX "vma is writeable\n");
   }
   else {
      printk(KERN_DEBUG LOG_PREFIX "vma is not writable\n");
   }

   if(vmf->flags & VM_WRITE || vmf->flags & VM_FAULT_WRITE)
   {
      printk(KERN_DEBUG LOG_PREFIX "write to page!\n");
   }

   /*if(vmf->pgoff != BCM283X_GPIO_PGOFF)
   {
      printk(KERN_ERR LOG_PREFIX "attempt to access outside the gpio registers: pgoff:%lu gpio-pgoff:%lu", vmf->pgoff, BCM283X_GPIO_PGOFF);
      return VM_FAULT_SIGBUS;
   }*/

   printk(KERN_DEBUG LOG_PREFIX "address=0x%lx\n", vmf->address - vma->vm_start);

   if(!(vmf->flags & FAULT_FLAG_WRITE))
      vma->vm_flags &= ~VM_WRITE;

   //vma->vm_flags &= ~VM_WRITE;
   //vma->vm_flags = (vma->vm_flags | VM_MAYREAD) & ~(VM_WRITE);

/*   addr = page_to_pfn(page) << PAGE_SHIFT;
   pgd = pgd_offset(vma->vm_mm, addr);
   p4d = p4d_offset(pgd, addr);
   pud = pud_offset(p4d, addr);
   pmd = pmd_offset(pud, addr);
   pte = pte_offset_map(pmd, addr);*/

//   tmp_pte = *pte;

//   set_pte(pte, pte_clear_flags(tmp_pte, _PAGE_PRESENT));
 //  set_pte(pte, pte_set_flags(tmp_pte, _PAGE_PROTNONE));

   //vma->vm_flags |= VM_WRITE;
   //vma->vm_flags &= ~VM_READ;

   vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));

   vmf->page = page;
   get_page(page);

   return 0;
}

int split(struct vm_area_struct * area, unsigned long addr)
{
   printk(KERN_DEBUG LOG_PREFIX "split!\n");
   return 0;
}

int mremap(struct vm_area_struct * area)
{
   printk(KERN_DEBUG LOG_PREFIX "mremap!\n");
   return 0;
}

void map_pages(struct vm_fault *vmf,
                  pgoff_t start_pgoff, pgoff_t end_pgoff)
{
   printk(KERN_DEBUG LOG_PREFIX "map_pages! start=%lu end=%lu\n", start_pgoff, end_pgoff);
}

/* notification that a previously read-only page is about to become
 * writable, if an error is returned it will cause a SIGBUS */
int mmap_mkwrite(struct vm_fault *vmf)
{
   struct vm_area_struct *vma = vmf->vma;

   printk(KERN_DEBUG LOG_PREFIX "page_mkwrite!\n");
   vma->vm_flags |= VM_WRITE;
   vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));

   lock_page(vmf->page);
   set_page_dirty(vmf->page);

   return VM_FAULT_LOCKED;
}

int pfn_mkwrite(struct vm_fault *vmf)
{
   printk(KERN_DEBUG LOG_PREFIX "pfn_mkwrite!\n");
   lock_page(vmf->page);
   set_page_dirty(vmf->page);
   return VM_FAULT_LOCKED;
}

/* called by access_process_vm when get_user_pages() fails, typically
 * for use by special VMAs that can switch between memory and hardware
 */
int mmap_access(struct vm_area_struct *vma, unsigned long addr,
              void *buf, int len, int write)
{
   printk(KERN_DEBUG LOG_PREFIX "access: addr=0x%lx len=%d write=%d\n", addr, len, write);
   return len;
}

struct vm_operations_struct gpiomem_dummy_mmap_vmops = {
   .open  = mmap_open,  /* mmap-open */
   .close = mmap_close, /* mmap-close */
   .fault = mmap_fault, /* fault handler */
   .page_mkwrite = mmap_mkwrite,
   .pfn_mkwrite = pfn_mkwrite,
   .access = mmap_access,
   .split = split,
   .mremap = mremap,
   .map_pages = map_pages
};
