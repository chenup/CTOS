#include <assert.h>
#include <stdio.h>
#include <kernel/panic.h>
#include <kernel/rtos_interrupt.h>
#include <kernel/rtos_glo.h>
#include <trace.h>

extern struct gic_data gic_data;
/* number of lists of IRQ hooks, one list per supported line. */
static irq_hook_t* irq_handlers[NR_IRQ_VECTORS] = {0};
/*
 * Is NULL defined?
 */
#if !defined(NULL)
#define NULL (void *)0
#endif
#define TRUE    1

/*===========================================================================*
 *        put_irq_handler            *
 *===========================================================================*/
/* Register an interrupt handler.  */
void put_irq_handler( irq_hook_t* hook, int irq,
  const irq_handler_t handler)
{
  int id;
  irq_hook_t **line;
  unsigned long bitmap;

  if( irq < 0 || irq >= NR_IRQ_VECTORS )
  //panic("invalid call to put_irq_handler: %d",  irq);
    panic("invalid call to put_irq_handler");

  line = &irq_handlers[irq];

  bitmap = 0;
  while ( *line != NULL ) {
  if(hook == *line) return; /* extra initialization */
  bitmap |= (*line)->id;  /* mark ids in use */
  line = &(*line)->next;
  }

  /* find the lowest id not in use */
  for (id = 1; id != 0; id <<= 1)
    if (!(bitmap & id)) break;

  if(id == 0)
    //panic("Too many handlers for irq: %d",  irq);
    panic("Too many handlers for irq");
  
  hook->next = NULL;
  hook->handler = handler;
  hook->irq = irq;
  hook->id = id;
  *line = hook;

  /* And as last enable the irq at the hardware.
   *
   * Internal this activates the line or source of the given interrupt.
   */
  if((irq_actids[hook->irq] &= ~hook->id) == 0) {
    //hw_intr_used(irq);
    //hw_intr_unmask(hook->irq);
    rtos_bsp_irq_unmask(&gic_data, hook->irq);
  }
}

/*===========================================================================*
 *        rm_irq_handler             *
 *===========================================================================*/
/* Unregister an interrupt handler.  */
void rm_irq_handler( const irq_hook_t* hook ) {
  const int irq = hook->irq; 
  const int id = hook->id;
  irq_hook_t **line;

  if( irq < 0 || irq >= NR_IRQ_VECTORS ) 
  //panic("invalid call to rm_irq_handler: %d",  irq);
    panic("invalid call to rm_irq_handler");
  /* remove the hook  */
  line = &irq_handlers[irq];
  while( (*line) != NULL ) {
  if((*line)->id == id) 
  {
    (*line) = (*line)->next;
    if (irq_actids[irq] & id)
      irq_actids[irq] &= ~id;
      }
      else {
    line = &(*line)->next;
      }
  }

  /* Disable the irq if there are no other handlers registered.
   * If the irq is shared, reenable it if there is no active handler.
   */
  if (irq_handlers[irq] == NULL) {
  //hw_intr_mask(irq);
    rtos_bsp_irq_mask(&gic_data, irq);
  //hw_intr_not_used(irq);
  }
  else if (irq_actids[irq] == 0) {
  //hw_intr_unmask(irq);
    rtos_bsp_irq_unmask(&gic_data, irq);
  }
}

/*===========================================================================*
 *        irq_handle             *
 *===========================================================================*/
/*
 * The function first disables interrupt is need be and restores the state at
 * the end. Before returning, it unmasks the IRQ if and only if all active ID
 * bits are cleared, and restart a process.
 */
void irq_handle(int irq)
{
  irq_hook_t * hook;

  /* here we need not to get this IRQ until all the handlers had a say */
  assert(irq >= 0 && irq < NR_IRQ_VECTORS);
  //hw_intr_mask(irq);
  rtos_bsp_irq_mask(&gic_data, irq);
  hook = irq_handlers[irq];

  /* Check for spurious interrupts. */
  DMSG("###DEBUG###: this is bug");
  if(hook == NULL) {
      static int nspurious[NR_IRQ_VECTORS], reportls_interval = 100;
      nspurious[irq]++;
      if(nspurious[irq] == 1 || !(nspurious[irq] % report_interval)) {
        //printf("irq_handle: spurious irq %d (count: %d); keeping masked\n",
    //irq, nspurious[irq]);
  if(report_interval < INT_MAX/2)
    report_interval *= 2;
      }
      return;
  }
  DMSG("###DEBUG###: cpu %" PRIu32, (uint32_t)get_core_pos());
  /* Call list of handlers for an IRQ. */
  while( hook != NULL ) {
    /* For each handler in the list, mark it active by setting its ID bit,
     * call the function, and unmark it if the function returns true.
     */
    irq_actids[irq] |= hook->id;
    
    /* Call the hooked function. */
    DMSG("###DEBUG###: cpu %" PRIu32, (uint32_t)get_core_pos());
    if( (*hook->handler)(hook) )
      irq_actids[hook->irq] &= ~hook->id;
    
    /* Next hooked function. */
    hook = hook->next;
  }
  DMSG("###DEBUG###: cpu %" PRIu32, (uint32_t)get_core_pos());
  /* reenable the IRQ only if there is no active handler */
  if (irq_actids[irq] == 0)
    //hw_intr_unmask(irq);
    rtos_bsp_irq_unmask(&gic_data, hook->irq);

  //hw_intr_ack(irq);
}

/* Enable/Disable a interrupt line.  */
void enable_irq(const irq_hook_t *hook)
{
  if((irq_actids[hook->irq] &= ~hook->id) == 0) {
    //hw_intr_unmask(hook->irq);
    rtos_bsp_irq_unmask(&gic_data, hook->irq);
  }
}

/* Return true if the interrupt was enabled before call.  */
int disable_irq(const irq_hook_t *hook)
{
  if(irq_actids[hook->irq] & hook->id)  /* already disabled */
    return 0;
  irq_actids[hook->irq] |= hook->id;
  //hw_intr_mask(hook->irq);
  rtos_bsp_irq_mask(&gic_data, hook->irq);
  return TRUE;
}

