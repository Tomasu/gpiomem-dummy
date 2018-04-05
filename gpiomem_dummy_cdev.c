#include "gpiomem_dummy_cdev.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm-generic/pgtable.h>

#include "gpiomem_dummy_log.h"

#include "gpiomem_dummy.h"
#include "gpiomem_dummy_mmap.h"
#include <linux/mman.h>

#define check_error(thing, fmt, ...) do { \
   typeof(thing) thing_copy_ = (thing); \
   if(!thing_copy_ || IS_ERR(thing_copy_)) \
   { \
      pr_err("check failed: " #thing " != NULL || IS_ERR(" #thing ")"); \
      pr_err(fmt, ##__VA_ARGS__); \
      return PTR_ERR(thing_copy_); \
   } \
} while(0)

#define check_error_cleanup(thing, fmt, ...) do { \
   typeof(thing) thing_copy_ = (thing); \
   if(!thing_copy_ || IS_ERR(thing_copy_)) \
   { \
      pr_err("check failed: " #thing " != NULL || IS_ERR(" #thing ")"); \
      pr_err(fmt, ##__VA_ARGS__); \
      error_ret = PTR_ERR(thing_copy_); \
      goto err_cleanup; \
   } \
} while(0)

#define check_state_cleanup(val, errcode, fmt, ...) do { \
   typeof(val) val_copy_ = (val); \
   if(val_copy_) \
   { \
      break; \
   } \
   \
   pr_err("check failed: " #val); \
   pr_err(fmt, ##__VA_ARGS__); \
   error_ret = errcode; \
   goto err_cleanup; \
} while(0)

#define check_val_cleanup(val, fmt, ...) do { \
   typeof(val) val_copy_ = (val); \
   if(val_copy_ == 0) \
   { \
      break; \
   } \
   \
   pr_err("check failed: " #val " == 0"); \
   pr_err(fmt, ##__VA_ARGS__); \
   error_ret = val_copy_; \
   goto err_cleanup; \
} while(0)

static int gd_cdev_set_page_ro(struct page *page)
{
   pgd_t *pgd;
   p4d_t *p4d;
   pud_t *pud;
   pmd_t *pmd;
   pte_t *pte;
   pte_t tmp_pte;

   unsigned long addr = 0;

   pr_info("check args");

   check_error(page, "invalid page");
   check_error(current, "no current task_struct");
   check_error(current->active_mm, "no active mm struct");

   pr_info("page_to_pfn");
   addr = page_to_phys(page);
   check_error((void*)addr, "invalid pfn");

   pr_info("pgd_offset");
   pgd = pgd_offset(current->active_mm, addr);
   if (pgd_none(*pgd) || pgd_bad(*pgd))
   {
      pgd_ERROR(*pgd);
      return -ENOMEM;
   }

   pr_info("p4d_offset");
   p4d = p4d_offset(pgd, addr);
   if (p4d_none(*p4d) || p4d_bad(*p4d))
   {
      p4d_ERROR(*p4d);
      return -ENOMEM;
   }

   pr_info("pud_offset");
   pud = pud_offset(p4d, addr);
   if (pud_none(*pud) || pud_bad(*pud))
   {
      pud_ERROR(*pud);
      return -ENOMEM;
   }

   pr_info("pmd_offset");
   pmd = pmd_offset(pud, addr);
   if (pmd_none(*pmd) || pmd_bad(*pmd))
   {
      pmd_ERROR(*pmd);
      return -ENOMEM;
   }

   pr_info("pte_offset_map");
   pte = pte_offset_map(pmd, addr);
   if (pte_none(*pte))
   {
      pte_ERROR(*pte);
      return -ENOMEM;
   }

   tmp_pte = *pte;

   pr_info("set pte");
   set_pte(pte, pte_clear_flags(tmp_pte, _PAGE_RW));

   pr_info("ret");
   return 0;
}

static int gd_cdev_open(struct inode *inodep, struct file *filep);
static int gd_cdev_mmap(struct file* file, struct vm_area_struct* vma);
static int gd_cdev_release(struct inode *inodep, struct file *filep);

static ssize_t gd_cdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t gd_cdev_write(struct file *, const char __user *, size_t, loff_t *);

static int gd_cdev_writepage(struct page *page, struct writeback_control *wbc);
static int gd_cdev_readpage(struct file *, struct page *);

/* Write back some dirty pages from this mapping. */
static int gd_cdev_writepages(struct address_space *, struct writeback_control *);

/* Set a page dirty.  Return true if this dirtied it */
static int gd_cdev_set_page_dirty(struct page *page);

static int gd_cdev_readpages(struct file *filp, struct address_space *mapping,
                   struct list_head *pages, unsigned nr_pages);

static int gd_cdev_write_begin(struct file *, struct address_space *mapping,
                     loff_t pos, unsigned len, unsigned flags,
                     struct page **pagep, void **fsdata);
static int gd_cdev_write_end(struct file *, struct address_space *mapping,
                   loff_t pos, unsigned len, unsigned copied,
                   struct page *page, void *fsdata);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations gd_cdev_fops =
{
   .owner = THIS_MODULE,
   .open = gd_cdev_open,
   .release = gd_cdev_release,
   .mmap = gd_cdev_mmap,
   .llseek = no_llseek,
   .read = gd_cdev_read,
   .write = gd_cdev_write,
};

struct address_space_operations gd_cdev_aops =
{
   .writepage = gd_cdev_writepage,
   .readpage = gd_cdev_readpage,
   .writepages = gd_cdev_writepages,
   .set_page_dirty = gd_cdev_set_page_dirty,
   .readpages = gd_cdev_readpages,
   .write_begin = gd_cdev_write_begin,
   .write_end = gd_cdev_write_end
};

int gd_cdev_init(struct gpiomem_dummy_cdev *cdev)
{
   int error_ret = -1;

   pr_info("init");
   check_error(cdev, "Null cdev?");

   memset(cdev, 0, sizeof(*cdev));

   pr_info("alloc page");
   struct page *page = alloc_page(GFP_USER | __GFP_ZERO);
   check_error(page, "failed to allocate page");

   unsigned long *pdata = (unsigned long*)page_to_virt(page);
   memset(pdata, 0, PAGE_SIZE);

   pr_info("set_page_ro");
   error_ret = gd_cdev_set_page_ro(page);
   check_val_cleanup(error_ret, "failed to set page ro");

   pr_info("register chrdev");
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   int cdev_major = register_chrdev(0, DEVICE_NAME, &gd_cdev_fops);
   check_state_cleanup(cdev_major >= 0, cdev_major, "failed to register chrdev major number");

   pr_info("registered major %d", cdev_major);

   // Register the device class
   struct class *clss = class_create(THIS_MODULE, CLASS_NAME);
   check_error_cleanup(clss, "failed to create class");

   pr_info("registered device class");

   // Register the device driver
   int dev_id = MKDEV(cdev_major, 0);

   struct device *dev = device_create(clss, NULL, dev_id, cdev, DEVICE_NAME);
   check_error_cleanup(dev, "failed to create device");

   cdev->page = page;
   cdev->major_number = cdev_major;
   cdev->clss = clss;
   cdev->dev_id = dev_id;
   cdev->dev = dev;

   pr_info("device created");
   return 0;

err_cleanup:
   if(dev && !IS_ERR(dev))
   {
      device_destroy(clss, dev_id);
   }

   if(clss && !IS_ERR(clss))
   {
      class_destroy(clss);
   }

   if(cdev_major)
   {
      unregister_chrdev(cdev_major, DEVICE_NAME);
   }

   if(page && !IS_ERR(page))
   {
      __free_page(page);
   }

   return error_ret;
}

void gd_cdev_destroy(struct gpiomem_dummy_cdev *cdev)
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

   if(cdev->page && !IS_ERR(cdev->page))
   {
      __free_page(cdev->page);
      cdev->page = NULL;
   }
}

inline static struct gpiomem_dummy_cdev *inode_to_cdev(struct inode *ip)
{
   return dev_get_drvdata(inode_to_cdev(ip)->dev);
}

inline static struct gpiomem_dummy_cdev *file_to_cdev(struct file *fp)
{
   return inode_to_cdev(fp->f_inode);
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int gd_cdev_open(struct inode *inodep, struct file *filep)
{
   struct gpiomem_dummy_cdev *cdev = inode_to_cdev(inodep);
   if(!cdev)
   {
      pr_err("cdev not found?!");
      return -ENODEV;
   }

   cdev->times_opened++;

   pr_info("device has been opened %d time(s)", cdev->times_opened);

   return nonseekable_open(inodep, filep);
}

static int gd_cdev_mmap(struct file* fp, struct vm_area_struct* vma)
{
   unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
   unsigned long size = vma->vm_end - vma->vm_start;
   struct gpiomem_dummy_cdev *cdev = NULL;
   struct page *page = NULL;

   pr_info("mmap request!");

   if (offset & ~PAGE_MASK)
   {
      pr_err("offset not aligned: %ld", offset);
      return -ENXIO;
   }

   /*if (vma->vm_pgoff != BCM283X_PERIPH_PGOFF)
   {
      pr_err("attempt to map outside of peripheral memory: vpgo=%d pbgo=%d", vma->vm_pgoff, BCM283X_PERIPH_PGOFF);
      return -ENXIO;
   }*/

   if (size > BCM283X_PERIPH_SIZE)
   {
      pr_err("size too big: %lu > %lu", size, BCM283X_PERIPH_SIZE);
      return -ENXIO;
   }

   if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
   {
      pr_err("writable mappings must be shared, rejecting");
      return(-EINVAL);
   }

   cdev = file_to_cdev(fp);
   if (!cdev)
   {
      pr_err("failed to get cdev!?");
      return -ENODEV;
   }

   page = cdev->page;

   vma->vm_private_data = cdev;

   vma->vm_file->f_mapping->a_ops = &gd_cdev_aops;

   vma->vm_ops = &gpiomem_dummy_mmap_vmops;

   vma->vm_ops->open(vma);

   pr_info("mmap success!");

   // gd_cdev_mmap_open(vma); <-- is it really necessary to call the mmap open function here if we have one set up?? I'd think it's already been called....

   return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int gd_cdev_release(struct inode *inodep, struct file *filep)
{
   pr_info("Device successfully closed");
   return 0;
}

static ssize_t gd_cdev_read(struct file *filp, char __user *data, size_t len, loff_t *offset)
{
   pr_info("read!");

   return len;
}

static ssize_t gd_cdev_write(struct file *filp, const char __user *data, size_t len, loff_t *offset)
{
   pr_info("write!");
   return len;
}

int gd_cdev_writepage(struct page *page, struct writeback_control *wbc)
{
   pr_info("writepage");
   return 0;
}

int gd_cdev_readpage(struct file *fp, struct page *pg)
{
   pr_info("readpage");
   return 0;
}

/* Write back some dirty pages from this mapping. */
int gd_cdev_writepages(struct address_space *as, struct writeback_control *wbc)
{
   pr_info("writepages");
   return 0;
}

//[31111.087184] BUG: unable to handle kernel NULL pointer dereference at 0000000000000010
//[31111.088182] IP: vmacache_find+0x23/0xa0

/* Set a page dirty.  Return true if this dirtied it */
int gd_cdev_set_page_dirty(struct page *page)
{
   int i = 0;
   int num = 0;
   char *data = page_to_virt(page);

   //unmap_mapping_range(page->mapping, 0, PAGE_SIZE, 1);

   for(i = 0; i < PAGE_SIZE; i++)
   {
      if(data[i]) num++;
      data[i] = 0;
   }

   pr_info("changes=%d", num);
   //page->mapping

   return !num;
}

int gd_cdev_readpages(struct file *filp, struct address_space *mapping,
                 struct list_head *pages, unsigned nr_pages)
{
   pr_info("readpages");
   return 0;
}

int gd_cdev_write_begin(struct file *filp, struct address_space *mapping,
                   loff_t pos, unsigned len, unsigned flags,
                   struct page **pagep, void **fsdata)
{
   pr_info("write_begin");
   return 0;
}

int gd_cdev_write_end(struct file *filp, struct address_space *mapping,
                 loff_t pos, unsigned len, unsigned copied,
                 struct page *page, void *fsdata)
{
   pr_info("write_end");
   return 0;
}
