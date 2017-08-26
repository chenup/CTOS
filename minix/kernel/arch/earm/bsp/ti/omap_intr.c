#include <sys/types.h>
#include <machine/cpu.h>
#include <minix/type.h>
#include <minix/board.h>
#include <io.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "kernel/vm.h"
#include "kernel/proto.h"
#include "arch_proto.h"
#include "hw_intr.h"

#include "omap_intr_registers.h"


static struct gic_data gic_data;
//TODO
//register_phys_mem(MEM_AREA_IO_SEC, GICD_BASE, GIC_DIST_REG_SIZE);
//register_phys_mem(MEM_AREA_IO_SEC, GICC_BASE, GIC_DIST_REG_SIZE);
int intr_init(const int auto_eoi __unused)
{
	/*
	vaddr_t gicc_base;
	vaddr_t gicd_base;
	//gicc_base = (vaddr_t)phys_to_virt(GIC_BASE + GICC_OFFSET,
	//				  MEM_AREA_IO_SEC);
	//gicd_base = (vaddr_t)phys_to_virt(GIC_BASE + GICD_OFFSET,
	//				  MEM_AREA_IO_SEC);
	if (!gicc_base || ！gicd_base)
	{
		panic();
	} 
	gic_init_base_addr(&gic_data, gicc_base, gicd_base);
	itr_init(&gic_data.chip);
	*/
	main_init_gic();
}

void
bsp_irq_handle(void)
{
	/*
	int irq =
	    mmio_read(omap_intr.base +
	    OMAP3_INTCPS_SIR_IRQ) & OMAP3_INTR_ACTIVEIRQ_MASK;

	irq_handle(irq);
	mmio_write(omap_intr.base + OMAP3_INTCPS_CONTROL,
	    OMAP3_INTR_NEWIRQAGR);
	*/
	//gic_it_handle(&gic_data);
	uint32_t iar;
	uint32_t id;
	iar = gic_read_iar(&gic_data);
	id = iar & GICC_IAR_IT_ID_MASK;

	if (id < (&gic_data)->max_it)
		//itr_handle(id);
	else
		DMSG("ignoring interrupt %" PRIu32, id);

	gic_write_eoir(&gic_data, iar);
}

//TODO
void bsp_irq_unmask(int irq)
{
	/*
	uint32_t idx = irq / NUM_INTS_PER_REG;
	uint32_t mask = 1 << (irq % NUM_INTS_PER_REG);
	assert(!(mmio_read(gic_data.gicd_base + GICD_IGROUPR(idx)) & mask));
	if(irq >= NUM_SGI)
	{
		assert(!(mmio_read(gic_data.gicd_base + GICD_ISENABLER(idx)) & mask));
	}
	mmio_write(gic_data.gicd_base + GICD_ISENABLER(idx), irq);
	*/
	gic_it_enable(&gic_data, irq);
}

//TODO
void bsp_irq_mask(const int irq)
{
	/*
	uint32_t idx = irq / NUM_INTS_PER_REG;
	uint32_t mask = 1 << (irq % NUM_INTS_PER_REG);
	assert(!(mmio_read(gic_data.gicd_base + GICD_IGROUPR(idx)) & mask));
	mmio_write(gic_data.gicd_base + GICD_ICENABLER(idx), irq);
	*/
	gic_it_disable(&gic_data, irq);
}