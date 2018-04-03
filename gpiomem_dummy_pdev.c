#include "gpiomem_dummy_pdev.h"

#include <linux/platform_device.h>

#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "pdev: "

struct resource io_resource = {
   .start = BCM283X_PERIPH_BASE,
   .end = BCM283X_PERIPH_BASE + BCM283X_PERIPH_SIZE,
   .flags = IORESOURCE_MEM
};

struct platform_device gpiomem_dummy_pdev = {
   .name = "gpiomem-pdev",
   .id = PLATFORM_DEVID_NONE,
   .num_resources = 1,
   .resource = &io_resource
};

static int pdrv_probe(struct platform_device *);
static int pdrv_remove(struct platform_device *);

struct platform_driver pdrv = {
   .probe = pdrv_probe,
   .remove = pdrv_remove
};


int gpiomem_dummy_pdrv_init(struct gpiomem_dummy_pdev *pdev)
{
   int pdr = platform_device_register(&gpiomem_dummy_pdev);
   if(pdr != 0)
   {
      printk(KERN_ERR LOG_PREFIX "failed to register device\n");
      return -ENODEV;
   }

   return 0;
}

void gpiomem_dummy_pdrv_exit(struct gpiomem_dummy_pdev *pdev)
{
   printk(KERN_DEBUG LOG_PREFIX "pdrv exit!\n");
}

int pdrv_probe(struct platform_device *pdev)
{
   printk(KERN_DEBUG LOG_PREFIX "new pdev!\n");

   if (!devm_request_mem_region(&pdev->dev, BCM283X_PERIPH_BASE,
      BCM283X_PERIPH_SIZE, pdev->name))
   {
      printk(KERN_ERR LOG_PREFIX "failed to allocate mem region for device\n");
      return -EBUSY;
   }

   return 0;
}


int pdrv_remove(struct platform_device *pdev)
{
   printk(KERN_DEBUG LOG_PREFIX "remove pdev!\n");

   return 0;
}
