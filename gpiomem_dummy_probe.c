#include "gpiomem_dummy_probe.h"

#include <asm/insn.h>
#include "inat.c"
#include "insn.c"

#include <linux/ptrace.h>
#include <asm/ptrace.h>
#include <linux/uprobes.h>
#include <linux/sched.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/sched/task_stack.h>
#include <linux/uaccess.h>
#include <asm/desc.h>
#include <asm/mmu_context.h>

#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "probe: "

static int gd_up_handler(struct uprobe_consumer *self, struct pt_regs *regs);

static struct uprobe_consumer gd_uprobe_consumer =
{
   .handler = gd_up_handler
};

static int get_next_ip(struct task_struct *task);
unsigned long insn_get_seg_base(struct pt_regs *regs, int seg_reg_idx);

void gd_add_probe(struct gpiomem_dummy *gd, struct gd_probe_info *task)
{
   list_add_rcu(&task->list, &gd->probe_list);
}

void gd_rem_task(struct gpiomem_dummy *gd, struct gd_probe_info *task)
{
   list_del_rcu(&task->list);
}

struct gd_probe_info *gd_create_probe(struct task_struct *task, struct vm_area_struct *vma)
{
   struct file *task_file = NULL;
   struct gd_probe_info *info = NULL;

   task_file = get_task_exe_file(task);
   if(!task_file || !task_file->f_inode)
   {
      pr_err("task is missing an associated file?!");
      return NULL;
   }

   info = kcalloc(1, sizeof(*info), GFP_KERNEL);

   info->inode = task_file->f_inode;
   info->task = task;
   info->ip = get_next_ip(task);

   info->vma = vma;

   return info;
}

int gd_register_probe(struct task_struct *task, struct vm_area_struct *vma)
{
   struct gd_probe_info *probe = NULL;
   int upret = 0;

   probe = gd_create_probe(task, vma);
   if (!probe)
   {
      return -ENOMEM;
   }

   upret = uprobe_register(probe->inode, probe->ip, &gd_uprobe_consumer);
   if (upret != 0)
   {
      pr_err("failed to register uprobe: %d\n", upret);
      return -ENOMEM;
   }

   gd_add_probe(gd_get_from_vma(vma), probe);

   return 0;
}

static int get_next_ip(struct task_struct *task)
{
   struct pt_regs *pt_regs = NULL;
   struct insn insn;
   char insn_buff[32];
   unsigned long seg_base = 0;
   int not_copied, nr_copied;

   pt_regs = task_pt_regs(task);
   /*
   * If not in user-space long mode, a custom code segment could be in
   * use. This is true in protected mode (if the process defined a local
   * descriptor table), or virtual-8086 mode. In most of the cases
   * seg_base will be zero as in USER_CS.
   */
   if (!user_64bit_mode(pt_regs))
      seg_base = insn_get_seg_base(pt_regs, INAT_SEG_REG_CS);

   if (seg_base == -1L)
   {
      pr_err("failed to get seg_base");
      return VM_FAULT_ERROR;
   }

   not_copied = copy_from_user(insn_buff, (void __user *)(seg_base + pt_regs->ip), sizeof(insn_buff));
   nr_copied = sizeof(insn_buff) - not_copied;

   /*
   * The copy_from_user above could have failed if user code is protected
   * by a memory protection key. Give up on this in such a case.
   * Should we issue a page fault?
   */
   if (!nr_copied)
   {
      pr_err("failed to copy instruction from userspace");
      return VM_FAULT_ERROR;
   }

   insn_init(&insn, insn_buff, nr_copied, user_64bit_mode(pt_regs));
   insn_get_length(&insn); // get length

   if (nr_copied < insn.length)
   {
      pr_err("failed to get instruction length");
      return VM_FAULT_ERROR;
   }

   return pt_regs->ip + insn.length;
}

static int gd_up_handler(struct uprobe_consumer *self, struct pt_regs *regs)
{
   struct gpiomem_dummy *gd = gd_get();

   if(!gd)
   {
      pr_err("invalid state >:(");
      return -ENOMEM;
   }

   /*rcu_read_lock();
   list_for_each_entry_rcu(probe, &gd->probe_list, list)
   {
      if(probe->ip = regs->ip)
      {
         pr_info("got probe!");

      }
   }
   rcu_read_unlock();
   */

   gd_set_page_ro(gd->page);

   return 0;
}


// dirty hack...

/**
 * get_segment_selector() - obtain segment selector
 * @regs:		Register values as seen when entering kernel mode
 * @seg_reg_idx:	Segment register index to use
 *
 * Obtain the segment selector from any of the CS, SS, DS, ES, FS, GS segment
 * registers. In CONFIG_X86_32, the segment is obtained from either pt_regs or
 * kernel_vm86_regs as applicable. In CONFIG_X86_64, CS and SS are obtained
 * from pt_regs. DS, ES, FS and GS are obtained by reading the actual CPU
 * registers. This done for only for completeness as in CONFIG_X86_64 segment
 * registers are ignored.
 *
 * Returns:
 *
 * Value of the segment selector, including null when running in
 * long mode.
 *
 * -EINVAL on error.
 */
static short get_segment_selector(struct pt_regs *regs, int seg_reg_idx)
{
   #ifdef CONFIG_X86_64
   unsigned short sel;

   switch (seg_reg_idx) {
      case INAT_SEG_REG_IGNORE:
         return 0;
      case INAT_SEG_REG_CS:
         return (unsigned short)(regs->cs & 0xffff);
      case INAT_SEG_REG_SS:
         return (unsigned short)(regs->ss & 0xffff);
      case INAT_SEG_REG_DS:
         savesegment(ds, sel);
         return sel;
      case INAT_SEG_REG_ES:
         savesegment(es, sel);
         return sel;
      case INAT_SEG_REG_FS:
         savesegment(fs, sel);
         return sel;
      case INAT_SEG_REG_GS:
         savesegment(gs, sel);
         return sel;
      default:
         return -EINVAL;
   }
   #else /* CONFIG_X86_32 */
   struct kernel_vm86_regs *vm86regs = (struct kernel_vm86_regs *)regs;

   if (v8086_mode(regs)) {
      switch (seg_reg_idx) {
         case INAT_SEG_REG_CS:
            return (unsigned short)(regs->cs & 0xffff);
         case INAT_SEG_REG_SS:
            return (unsigned short)(regs->ss & 0xffff);
         case INAT_SEG_REG_DS:
            return vm86regs->ds;
         case INAT_SEG_REG_ES:
            return vm86regs->es;
         case INAT_SEG_REG_FS:
            return vm86regs->fs;
         case INAT_SEG_REG_GS:
            return vm86regs->gs;
         case INAT_SEG_REG_IGNORE:
            /* fall through */
            default:
               return -EINVAL;
      }
   }

   switch (seg_reg_idx) {
      case INAT_SEG_REG_CS:
         return (unsigned short)(regs->cs & 0xffff);
      case INAT_SEG_REG_SS:
         return (unsigned short)(regs->ss & 0xffff);
      case INAT_SEG_REG_DS:
         return (unsigned short)(regs->ds & 0xffff);
      case INAT_SEG_REG_ES:
         return (unsigned short)(regs->es & 0xffff);
      case INAT_SEG_REG_FS:
         return (unsigned short)(regs->fs & 0xffff);
      case INAT_SEG_REG_GS:
         /*
          * GS may or may not be in regs as per CONFIG_X86_32_LAZY_GS.
          * The macro below takes care of both cases.
          */
         return get_user_gs(regs);
      case INAT_SEG_REG_IGNORE:
         /* fall through */
         default:
            return -EINVAL;
   }
   #endif /* CONFIG_X86_64 */
}

/**
 * get_desc() - Obtain pointer to a segment descriptor
 * @sel: Segment selector
 *
 * Given a segment selector, obtain a pointer to the segment descriptor.
 * Both global and local descriptor tables are supported.
 *
 * Returns:
 *
 * Pointer to segment descriptor on success.
 *
 * NULL on error.
 */
static struct desc_struct *get_desc(unsigned short sel)
{
   struct desc_ptr gdt_desc = {0, 0};
   unsigned long desc_base;

   #ifdef CONFIG_MODIFY_LDT_SYSCALL
   if ((sel & SEGMENT_TI_MASK) == SEGMENT_LDT) {
      struct desc_struct *desc = NULL;
      struct ldt_struct *ldt;

      /* Bits [15:3] contain the index of the desired entry. */
      sel >>= 3;

      mutex_lock(&current->active_mm->context.lock);
      ldt = current->active_mm->context.ldt;
      if (ldt && sel < ldt->nr_entries)
         desc = &ldt->entries[sel];

      mutex_unlock(&current->active_mm->context.lock);

      return desc;
   }
   #endif
   native_store_gdt(&gdt_desc);

   /*
    * Segment descriptors have a size of 8 bytes. Thus, the index is
    * multiplied by 8 to obtain the memory offset of the desired descriptor
    * from the base of the GDT. As bits [15:3] of the segment selector
    * contain the index, it can be regarded as multiplied by 8 already.
    * All that remains is to clear bits [2:0].
    */
   desc_base = sel & ~(SEGMENT_RPL_MASK | SEGMENT_TI_MASK);

   if (desc_base > gdt_desc.size)
      return NULL;

   return (struct desc_struct *)(gdt_desc.address + desc_base);
}

unsigned long insn_get_seg_base(struct pt_regs *regs, int seg_reg_idx)
{
   struct desc_struct *desc;
   short sel;

   sel = get_segment_selector(regs, seg_reg_idx);
   if (sel < 0)
      return -1L;

   if (v8086_mode(regs))
      /*
       * Base is simply the segment selector shifted 4
       * bits to the right.
       */
      return (unsigned long)(sel << 4);

   if (user_64bit_mode(regs)) {
      /*
       * Only FS or GS will have a base address, the rest of
       * the segments' bases are forced to 0.
       */
      unsigned long base;

      if (seg_reg_idx == INAT_SEG_REG_FS)
         rdmsrl(MSR_FS_BASE, base);
      else if (seg_reg_idx == INAT_SEG_REG_GS)
         /*
          * swapgs was called at the kernel entry point. Thus,
          * MSR_KERNEL_GS_BASE will have the user-space GS base.
          */
         rdmsrl(MSR_KERNEL_GS_BASE, base);
      else
         base = 0;
      return base;
   }

   /* In protected mode the segment selector cannot be null. */
   if (!sel)
      return -1L;

   desc = get_desc(sel);
   if (!desc)
      return -1L;

   return get_desc_base(desc);
}
