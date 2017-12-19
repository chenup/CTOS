#include <rtos_wait.h>
#include <compiler.h>
#include <trace.h>
//TODO
#include <rtos_sched.h>

void sn_thread_sched(void);

void wakeup(unsigned long data __unused)
{
	__wake_up();
}

void __wake_up(void)
{
	DMSG("#####wakeup#####");
	sn_thread_sched();
}

void pause(void)
{
	sn_thread_sched();
}

//TODO
void scheduler_tick(void)
{
	process_sched();
}