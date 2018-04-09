#ifndef GPIOMEM_DUMMY_CDEV_H_GUARD
#define GPIOMEM_DUMMY_CDEV_H_GUARD

#include "gpiomem_dummy_log.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>

struct gpiomem_dummy_cdev
{
   struct cdev cdev;
   int major_number;
   int dev_id;
   int times_opened;

   /* just so we can have the char device auto create in /dev ... sigh */
   struct class *clss;
   struct device *dev;

};

extern int gd_cdev_init(struct gpiomem_dummy_cdev *cdev);
extern void gd_cdev_destroy(struct gpiomem_dummy_cdev *cdev);

extern struct address_space_operations cdev_aops;

#endif /* GPIOMEM_DUMMY_CDEV_H_GUARD */
