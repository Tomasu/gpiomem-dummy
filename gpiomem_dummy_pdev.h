#ifndef GPIOMEM_DUMMY_PDEV_H_GUARD
#define GPIOMEM_DUMMY_PDEV_H_GUARD

struct gpiomem_dummy_pdev
{
   // nada
};

int gpiomem_dummy_pdrv_init(struct gpiomem_dummy_pdev *pdev);
void gpiomem_dummy_pdrv_exit(struct gpiomem_dummy_pdev *pdev);

#endif /* GPIOMEM_DUMMY_PDEV_H_GUARD */
