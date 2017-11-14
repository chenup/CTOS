#ifndef RTOS_FIQ_CPUSTAT_H
#define RTOS_FIQ_CPUSTAT_H

#include <rtos_hardfiq.h>
#include <kernel/misc.h>

//extern fiq_cpustat_t fiq_stat[];
#define __FIQ_STAT(cpu, member)	(fiq_stat[cpu].member)


//changed
#define local_softfiq_pending() \
	__FIQ_STAT(get_core_pos(), __softfiq_pending)

#endif