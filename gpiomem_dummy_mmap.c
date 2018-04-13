#include "gpiomem_dummy_mmap.h"

#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

#include "gpiomem_dummy.h"
#include "gpiomem_dummy_probe.h"

#define LOG_PREFIX LOG_PREFIX_ "mmap: "

/* memory handler functions */


static void mmap_open(struct vm_area_struct* vma)
{
   struct mm_struct *mm = NULL;

   printk(KERN_DEBUG LOG_PREFIX "mmap_open\n");

   mm = vma->vm_mm;
   if (!mm) {
      pr_err("vma vm_mm is missing :(");
      return;
   }

   //vma->vm_flags = (vma->vm_flags | VM_DONTEXPAND | VM_DONTCOPY | VM_DONTDUMP | VM_IO | VM_MAYREAD | VM_MIXEDMAP);
   vma->vm_flags |= VM_DONTCOPY | VM_DONTDUMP | VM_DONTEXPAND | VM_SHARED | VM_LOCKED | VM_WRITE | VM_MIXEDMAP;
   //vma->vm_flags &= ~(VM_MAYWRITE /*| VM_WRITE*/);
   //vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));

   //if (!vma->vm_mm->exe_file || !vma)
   //uprobe_register(vma->vm_mm->exe_file->f_inode, 0, NULL);
}

static void mmap_close(struct vm_area_struct* vma)
{
   // do nothing
   printk(KERN_DEBUG LOG_PREFIX "mmap_close\n");
}

static inline
struct gpiomem_dummy *vmf_get_gd(struct vm_fault *vmf)
{
   return vmf && vmf->vma ? vmf->vma->vm_private_data : NULL;
}

static inline
struct page *vmf_get_page(struct vm_fault *vmf)
{
   struct gpiomem_dummy *gd = NULL;

   if(vmf->page)
   {
      return vmf->page;
   }

   gd = vmf_get_gd(vmf);

   return gd ? gd->page : NULL;
}

void hw_breakpoint_trigger(struct perf_event *event, struct perf_sample_data *data, struct pt_regs *regs)
{
   pr_info("hw hw_breakpoint!");
   unregister_hw_breakpoint(event);
}

static int mmap_fault(struct vm_fault* vmf)
{
   struct page *page = NULL;
   struct vm_area_struct *vma = vmf->vma;
   struct perf_event_attr pe_attr;
   int insret = -ENODEV;

   printk(KERN_DEBUG LOG_PREFIX "fault: pgoff=0x%lx addr=0x%lx pte=%p\n", vmf->pgoff, vmf->address - vma->vm_start, vmf->pte);

   printk("fault: flags ");
   if(vmf->flags & FAULT_FLAG_INSTRUCTION)
      printk(KERN_CONT "INST ");
   if(vmf->flags & FAULT_FLAG_KILLABLE)
      printk(KERN_CONT "KILLABLE ");
   if(vmf->flags & FAULT_FLAG_MKWRITE)
      printk(KERN_CONT "MKWRITE ");
   if(vmf->flags & FAULT_FLAG_REMOTE)
      printk(KERN_CONT "REMOTE ");
   if(vmf->flags & FAULT_FLAG_TRIED)
      printk(KERN_CONT "TRIED ");
   if(vmf->flags & FAULT_FLAG_USER)
      printk(KERN_CONT "USER ");
   if(vmf->flags & FAULT_FLAG_WRITE)
      printk(KERN_CONT "WRITE ");

   printk("");

   if(vmf->pte)
   {
      pr_info("we have a pte!");
   }
   else
   {
      pr_info("we do not have a pte!");
   }

   page = vmf_get_page(vmf);

   if(!page || IS_ERR(page))
   {
      pr_err("unable to get mem pointer");
      return VM_FAULT_SIGBUS;
   }

   if(vma->vm_flags & VM_WRITE)
   {
      pr_info("vma is writeable");
   }
   else {
      pr_info("vma is not writable");
   }

   if(vmf->flags & FAULT_FLAG_WRITE)
   {
      pr_info("write to page");
   }
   else
   {
      pr_info("read from page");
   }

   // set page rw for now
   // gd_set_page_rw(page);


   //ptrace_breakpoint_init(&pe_attr);
   //pe_attr.bp_addr =
   //register_user_hw_breakpoint(&pe_attr, hw_breakpoint_trigger, NULL /* user data */, current);

   insret = vm_insert_page(vma, vmf->address, page);
   if (insret != 0)
   {
      pr_err("failed to insert page: %d", insret);
      return VM_FAULT_ERROR;
   }

   // register probe on /next/ instruction, then we'll mark page ro again after
   if(gd_register_probe(current, vma) != 0)
   {
      pr_err("failed to register gd probe");
      return VM_FAULT_ERROR;
   }

   //vmf->page = page;
   //get_page(page);
   //lock_page(page);

   pr_info("mmap_fault done");
   return VM_FAULT_NOPAGE;
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
   /*pgd_t *pgd;
   p4d_t *p4d;
   pud_t *pud;
   pmd_t *pmd;
   pte_t *pte;
   unsigned long addr;
*/
   printk(KERN_DEBUG LOG_PREFIX "map_pages! start=%lu end=%lu\n", start_pgoff, end_pgoff);

  /* addr = vmf->address;

   pgd = pgd_offset(vmf->vma->vm_mm, addr);
   p4d = p4d_offset(pgd, addr);
   pud = pud_offset(p4d, addr);
   pmd = pmd_offset(pud, addr);
   pte_alloc_map()
   vmf->pte = pte_offset_map(pmd, addr);*/
}

/* notification that a previously read-only page is about to become
 * writable, if an error is returned it will cause a SIGBUS */
int mmap_mkwrite(struct vm_fault *vmf)
{
   struct vm_area_struct *vma = vmf->vma;

   printk(KERN_DEBUG LOG_PREFIX "page_mkwrite!\n");
   //vma->vm_flags |= VM_WRITE;
   //vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));

   lock_page(vmf->page);
   //set_page_dirty(vmf->page);

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
