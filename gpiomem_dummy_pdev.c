#include "gpiomem_dummy_pdev.h"

#include <linux/platform_device.h>

#include "gpiomem_dummy.h"

#define LOG_PREFIX LOG_PREFIX_ "pdev: "

static int pdrv_probe(struct platform_device *);
static int pdrv_remove(struct platform_device *);
static void pdrv_shutdown(struct platform_device *);
static void pdrv_release(struct device *);

struct platform_driver pdrv = {
   .driver = {
      .name = "gd-pdev",
      .owner = THIS_MODULE,
   },
   .probe = pdrv_probe,
   .remove = pdrv_remove,
   .shutdown = pdrv_shutdown
};

struct resource io_resource = {
   .start = BCM283X_PERIPH_BASE,
   .end = BCM283X_PERIPH_BASE + BCM283X_PERIPH_SIZE,
   .flags = IORESOURCE_MEM
};

struct platform_device gd_pdev = {
   .name = "gd-pdev",
   .id = PLATFORM_DEVID_NONE,
   .num_resources = 1,
   .resource = &io_resource,
   .dev = {
      .release = pdrv_release
   }
};

struct gpiomem_dummy *gd_from_platdev(struct platform_device *platdev)
{
   return container_of(platform_get_drvdata(platdev), struct gpiomem_dummy, pdev);
}

struct gd_pdev *platdev_to_gdpdev(struct platform_device *platdev)
{
   return platform_get_drvdata(platdev);
}

int gd_pdrv_init(struct gd_pdev *pdev)
{
   int drv_ret = -1;
   int pdr = -1;

   pdev->plat_dev = gd_pdev;

   pdr = platform_device_register(&pdev->plat_dev);
   if(pdr != 0)
   {
      pr_err("failed to register device");
      return pdr;
   }

   drv_ret = platform_driver_probe(&pdrv, pdrv_probe);
   if(drv_ret != 0)
   {
      pr_err("failed to probe platform driver");
      platform_device_unregister(&pdev->plat_dev);
      return drv_ret;
   }

   return 0;
}

void gd_pdrv_exit(struct gd_pdev *pdev)
{
   printk(KERN_DEBUG LOG_PREFIX "pdrv exit!\n");
}

int pdrv_probe(struct platform_device *pdev)
{
   unsigned long __iomem *data = NULL;
   struct resource *res = NULL;
   struct platform_device *platdev = &gd_pdev;
   struct gpiomem_dummy *gd = gd_from_platdev(platdev);

   res = platform_get_resource(platdev, IORESOURCE_MEM, 0);
   if (IS_ERR(res))
   {
      pr_err("failed to get mem resource");
      return PTR_ERR(res);
   }

   data = devm_ioremap_resource(&platdev->dev, res);
   if (IS_ERR(data))
   {
      pr_err("failed to map resource.");
      return PTR_ERR(data);
   }

   gd_from_platdev(platdev)->pdev.iomem = data;

   printk(KERN_DEBUG LOG_PREFIX "new pdev!\n");

   *data = 1234;

   /*
   if (!devm_request_mem_region(&pdev->dev, BCM283X_PERIPH_BASE,
      BCM283X_PERIPH_SIZE, pdev->name))
   {
      printk(KERN_ERR LOG_PREFIX "failed to allocate mem region for device\n");
      return -EBUSY;
   }*/

   return 0;
}


int pdrv_remove(struct platform_device *pdev)
{
   pr_info("remove pdev!");

   return 0;
}

void pdrv_shutdown(struct platform_device *pdev)
{
   pr_info("shutdown pdev!");
}

void pdrv_release(struct device *dev)
{
   pr_info("release dev!");
}
