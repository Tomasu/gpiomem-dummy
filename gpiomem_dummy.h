#ifndef GPIOMEM_DUMMY_H_GUARD
#define GPIOMEM_DUMMY_H_GUARD

#include "gpiomem_dummy_cdev.h"
#include "gpiomem_dummy_procfs.h"

#define DEVICE_NAME "gpiomem"    ///< The device will appear at /dev/gpiomem using this value
#define CLASS_NAME  "gpiomem"        ///< The device class -- this is a character device driver

#define LOG_PREFIX_ "gpiomem-dummy: "

#define BCM283X_PERIPH_BASE 0x7E000000Lu
#define BCM283X_PERIPH_PGOFF (BCM283X_PERIPH_BASE >> PAGE_SHIFT)
#define BCM283X_PERIPH_SIZE 0x1000000Lu

#define BCM283X_GPIO_OFFSET 0x200000Lu
#define BCM283X_GPIO_BASE (BCM283X_PERIPH_BASE + BCM283X_GPIO_OFFSET)
#define BCM283X_GPIO_SIZE 0xb4Lu

#define BCM283X_GPIO_PGOFF (BCM283X_GPIO_OFFSET >> PAGE_SHIFT)

#define KALLOC_MEM_SIZE PAGE_SIZE

struct gpiomem_dummy
{
   int initialized;
   struct gpiomem_dummy_cdev cdev; // our char device state.
   struct gpiomem_dummy_procfs proc; // our proc device state;

   void *kmalloc_ptr;  // our single gpio mem page
   void *kmalloc_area; // a second pointer, because reasons?
};

extern void *dummy_get_mem_ptr(void);
extern struct gpiomem_dummy *dummy_get(void);

#endif /* GPIOMEM_DUMMY_H_GUARD */
