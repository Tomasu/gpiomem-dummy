#ifndef GPIOMEM_DUMMY_CDEV_H_GUARD
#define GPIOMEM_DUMMY_CDEV_H_GUARD

#include <linux/device.h>

struct gpiomem_dummy_cdev
{
   int major_number;
   int dev_id;
   int times_opened;
   struct class *clss;
   struct device *dev;
};

extern int gpiomem_dummy_cdev_init(struct gpiomem_dummy_cdev *cdev);
extern void gpiomem_dummy_cdev_destroy(struct gpiomem_dummy_cdev *cdev);

extern struct address_space_operations cdev_aops;

#endif /* GPIOMEM_DUMMY_CDEV_H_GUARD */
