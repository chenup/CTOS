#include <rtos_jiffies.h>
#include <rtos_time.h>

//u64 jiffies_64 __cacheline_aligned_in_smp = INITIAL_JIFFIES;
u64 jiffies_64 = INITIAL_JIFFIES;
//EXPORT_SYMBOL(jiffies_64);
//TODO

//#define jiffies jiffies_64

void timer_trick(void)
{
	do_timer(1);
	update_process_times();
}



