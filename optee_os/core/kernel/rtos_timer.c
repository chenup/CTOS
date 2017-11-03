#include <rtos_jiffies.h>
#include <rtos_time.h>
#include <rtos_timer.h>
#include <trace.h>

struct tvec {
	struct list_head vec[TVN_SIZE];
};

struct tvec_root {
	struct list_head vec[TVR_SIZE];
};

struct tvec_base {
	//spinlock_t lock;
	struct timer_list *running_timer;
	unsigned long timer_jiffies;
	struct tvec_root tv1;
	struct tvec tv2;
	struct tvec tv3;
	struct tvec tv4;
	struct tvec tv5;
};

struct tvec_base boot_tvec_bases;

void add_timer(struct timer_list *timer)
{
	//BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}

static inline int
__mod_timer(struct timer_list *timer __unused, unsigned long expires __unused, bool pending_only __unused)
{
	struct tvec_base *base, *new_base;
	unsigned long flags;
	int ret;

	ret = 0;
	//DMSG("###DEBUG###: jiffies_64 %" PRIu32, (uint32_t)expires);
	//DMSG("###DEBUG###: jiffies_64 %" PRIu32, (uint32_t)pending_only);
#ifdef BAD
	timer_stats_timer_set_start_info(timer);
	BUG_ON(!timer->function);

	base = lock_timer_base(timer, &flags);

	if (timer_pending(timer)) {
		detach_timer(timer, 0);
		ret = 1;
	} else {
		if (pending_only)
			goto out_unlock;
	}

	debug_timer_activate(timer);

	new_base = __get_cpu_var(tvec_bases);

	if (base != new_base) {
		/*
		 * We are trying to schedule the timer on the local CPU.
		 * However we can't change timer's base while it is running,
		 * otherwise del_timer_sync() can't detect that the timer's
		 * handler yet has not finished. This also guarantees that
		 * the timer is serialized wrt itself.
		 */
		if (likely(base->running_timer != timer)) {
			/* See the comment in lock_timer_base() */
			timer_set_base(timer, NULL);
			spin_unlock(&base->lock);
			base = new_base;
			spin_lock(&base->lock);
			timer_set_base(timer, base);
		}
	}

	timer->expires = expires;
	internal_add_timer(base, timer);

out_unlock:
	spin_unlock_irqrestore(&base->lock, flags);
#endif
	return ret;
}

int mod_timer(struct timer_list *timer, unsigned long expires)
{
	/*
	 * This is a common optimization triggered by the
	 * networking code - if the timer is re-modified
	 * to be the same thing then just return:
	 */
	if (timer->expires == expires && timer_pending(timer))
		return 1;

	return __mod_timer(timer, expires, false);
}

static inline void update_times(unsigned long ticks )
{
	DMSG("###DEBUG###: jiffies_64 %" PRIu32, (uint32_t)ticks);
	//update_wall_time();
	//calc_load(ticks);
}

void do_timer(unsigned long ticks)
{
	jiffies_64 += ticks;
	update_times(ticks);
}