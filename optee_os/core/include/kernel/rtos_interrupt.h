#ifndef RTOS_INTERRUPT_H
#define RTOS_INTERRUPT_H

#include <drivers/rtos_bsp_intr.h>
#include <drivers/gic.h>
#include <kernel/rtos_type.h>
#define NR_IRQ_VECTORS    1023
void put_irq_handler(irq_hook_t* hook, int irq, const irq_handler_t handler);
void enable_irq(const irq_hook_t *hook);
int disable_irq(const irq_hook_t *hook);
void irq_handle(int irq);
void rm_irq_handler(const irq_hook_t* hook);
#endif 