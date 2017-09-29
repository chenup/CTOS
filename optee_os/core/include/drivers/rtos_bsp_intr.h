#ifndef RTOS_BSP_INTR_H
#define RTOS_BSP_INTR_H

struct gic_data;
void rtos_bsp_irq_unmask(struct gic_data *gd, int fiq);
void rtos_bsp_irq_mask(struct gic_data *gd, int fiq);
void rtos_bsp_irq_handle(struct gic_data *gd);

#endif