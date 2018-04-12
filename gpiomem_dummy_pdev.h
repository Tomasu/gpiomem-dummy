#ifndef GPIOMEM_DUMMY_PDEV_H_GUARD
#define GPIOMEM_DUMMY_PDEV_H_GUARD

#include <linux/platform_device.h>

struct gd_pdev
{
   struct platform_device plat_dev;
   void __iomem *iomem;
};

int gd_pdrv_init(struct gd_pdev *pdev);
void gd_pdrv_exit(struct gd_pdev *pdev);

#endif /* GPIOMEM_DUMMY_PDEV_H_GUARD */
