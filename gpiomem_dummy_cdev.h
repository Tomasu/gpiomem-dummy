#ifndef GPIOMEM_DUMMY_CDEV_H_GUARD
#define GPIOMEM_DUMMY_CDEV_H_GUARD

#include <linux/device.h>

struct gpiomem_dummy_cdev
{
   int major_number;
   int times_opened;
   struct class *clss;
   struct device *dev;
};

extern int gpiomem_dummy_cdev_create(struct gpiomem_dummy_cdev *cdev);
extern void gpiomem_dummy_cdev_destroy(struct gpiomem_dummy_cdev *cdev);

#endif /* GPIOMEM_DUMMY_CDEV_H_GUARD */
