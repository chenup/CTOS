#include <kernel/interrupt.h>
#include <trace.h>
#include <rtos_hardfiq.h>
#include <rtos_fiq_cpustat.h>
#include <stdint.h>
#include <kernel/misc.h>

#define invoke_softfiq()	do_softfiq()

static struct softfiq_action softfiq_vec[NR_SOFTIRQS];

void raise_softfiq(unsigned int nr)
{
	//unsigned long flags;

	//local_irq_save(flags);
	raise_softfiq_fiqoff(nr);
	//local_irq_restore(flags);
}

/*
 * This function must run with irqs disabled!
 */
inline void raise_softfiq_fiqoff(unsigned int nr)
{
	__raise_softfiq_fiqoff(nr);
	
	if (!in_interrupt())
		wakeup_softfiqd();
}

void wakeup_softfiqd(void)
{
	DMSG("\n#debug# wakeup_softfiqd\n");
}

void open_softfiq(int nr, void (*action)(void))
{
	softfiq_vec[nr].action = action;
}

void fiq_enter(void)
{
	add_fiq_count();
	DMSG("\n#debug# fiq enter\n");
}

void fiq_exit(void)
{
	sub_fiq_count();
	if (!in_interrupt() && local_softfiq_pending())
	{
		invoke_softfiq();
	}
	DMSG("\n#debug# fiq exit\n");
}

void do_softfiq(void)
{
	uint32_t pending;
	//unsigned long flags;

	if (in_interrupt())
		return;

	//local_irq_save(flags);

	pending = local_softfiq_pending();

	if (pending)
		__do_softfiq();

	//local_irq_restore(flags);
}

#define MAX_SOFTFIQ_RESTART 10
void __do_softfiq(void)
{
	struct softfiq_action *h;
	uint32_t pending;
	int max_restart = MAX_SOFTFIQ_RESTART;
	//int cpu;

	pending = local_softfiq_pending();
	//account_system_vtime(current);

	//__local_bh_disable((unsigned long)__builtin_return_address(0));
	//lockdep_softirq_enter();

	//cpu = get_core_pos();
restart:
	/* Reset the pending bitmask before enabling fiqs */
	set_softfiq_pending(0);

	//local_irq_enable();

	h = softfiq_vec;

	do {
		if (pending & 1) {
			//int prev_count = preempt_count();

			//trace_softirq_entry(h, softirq_vec);
			h->action();
			//trace_softirq_exit(h, softirq_vec);
			/*
			if (unlikely(prev_count != preempt_count())) {
				printk(KERN_ERR "huh, entered softirq %td %s %p"
				       "with preempt_count %08x,"
				       " exited with %08x?\n", h - softirq_vec,
				       softirq_to_name[h - softirq_vec],
				       h->action, prev_count, preempt_count());
				preempt_count() = prev_count;
			}
			*/
			//rcu_bh_qsctr_inc(cpu);
		}
		h++;
		pending >>= 1;
	} while (pending);

	//local_irq_disable();

	pending = local_softfiq_pending();
	if (pending && --max_restart)
		goto restart;

	if (pending)
		wakeup_softfiqd();

	//lockdep_softirq_exit();

	//account_system_vtime(current);
	//_local_bh_enable();
}
