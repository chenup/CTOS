#include <drivers/gic.h>
#include <drivers/rtos_bsp_intr.h>
#include <compiler.h>

void rtos_bsp_irq_unmask(struct gic_data *gd, int fiq)
{
	gic_it_enable(gd, fiq);
}

void rtos_bsp_irq_mask(struct gic_data *gd, int fiq)
{
	gic_it_disable(gd, fiq);
}

void rtos_bsp_irq_handle(struct gic_data *gd)
{
	gic_it_handle(gd);
}
