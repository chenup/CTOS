#include <rtos_jiffies.h>
#include <rtos_time.h>
#include <rtos_timer.h>
#include <trace.h>
#include <rtos_percpu.h>
#include <rtos_poison.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>

#define jiffies jiffies_64
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

static void __init_timer(struct timer_list *timer);
static void internal_add_timer(struct tvec_base *base, struct timer_list *timer);
static void detach_timer(struct timer_list *timer, int clear_pending);
static int __mod_timer(struct timer_list *timer, unsigned long expires, bool pending_only);
static void update_times(unsigned long ticks );
static void run_timer_softfiq(void);
static int  init_timers_cpu(void);
static void __run_timers(struct tvec_base *base);
static int cascade(struct tvec_base *base, struct tvec *tv, int index);
static void set_running_timer(struct tvec_base *base, struct timer_list *timer);

struct tvec_base boot_tvec_bases;
static struct tvec_base* per_cpu__tvec_bases = &boot_tvec_bases;

static inline void detach_timer(struct timer_list *timer, int clear_pending)
{
	struct list_head *entry = &timer->entry;

	//debug_timer_deactivate(timer);

	__list_del(entry->prev, entry->next);
	if (clear_pending)
		entry->next = NULL;
	entry->prev = LIST_POISON2;
}

static void internal_add_timer(struct tvec_base *base, struct timer_list *timer)
{
	unsigned long expires = timer->expires;
	unsigned long idx = expires - base->timer_jiffies;
	struct list_head *vec;

	if (idx < TVR_SIZE) {
		int i = expires & TVR_MASK;
		vec = base->tv1.vec + i;
	} else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
		int i = (expires >> TVR_BITS) & TVN_MASK;
		vec = base->tv2.vec + i;
	} else if (idx < 1 << (TVR_BITS + 2 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
		vec = base->tv3.vec + i;
	} else if (idx < 1 << (TVR_BITS + 3 * TVN_BITS)) {
		int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
		vec = base->tv4.vec + i;
	} else if ((signed long) idx < 0) {
		/*
		 * Can happen if you add a timer with expires == jiffies,
		 * or you set a timer to go off in the past
		 */
		vec = base->tv1.vec + (base->timer_jiffies & TVR_MASK);
	} else {
		int i;
		/* If the timeout is larger than 0xffffffff on 64-bit
		 * architectures then we use the maximum timeout:
		 */
		if (idx > 0xffffffffUL) {
			idx = 0xffffffffUL;
			expires = idx + base->timer_jiffies;
		}
		i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
		vec = base->tv5.vec + i;
	}
	/*
	 * Timers are FIFO:
	 */
	list_add_tail(&timer->entry, vec);
}

void add_timer(struct timer_list *timer)
{
	//BUG_ON(timer_pending(timer));
	mod_timer(timer, timer->expires);
}

static inline int __mod_timer(struct timer_list *timer, unsigned long expires, bool pending_only)
{
	//struct tvec_base *base, *new_base;
	struct tvec_base *base;
	//unsigned long flags;
	int ret;

	ret = 0;
	//DMSG("###DEBUG###: jiffies_64 %" PRIu32, (uint32_t)expires);
	//DMSG("###DEBUG###: jiffies_64 %" PRIu32, (uint32_t)pending_only);
	//timer_stats_timer_set_start_info(timer);
	//BUG_ON(!timer->function);

	//base = lock_timer_base(timer, &flags);
	base = timer->base;
	if (timer_pending(timer)) {
		detach_timer(timer, 0);
		ret = 1;
	} else {
		if (pending_only)
			goto out_unlock;
	}

	//debug_timer_activate(timer);

	//new_base = __get_cpu_var(tvec_bases);
#ifdef RTOS_BAD
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
#endif
	timer->expires = expires;
	internal_add_timer(base, timer);

out_unlock:
	//spin_unlock_irqrestore(&base->lock, flags);
	return ret;
}

void init_timer(struct timer_list *timer)
{
	__init_timer(timer);
}


static void __init_timer(struct timer_list *timer)
{
	timer->entry.next = NULL;
	timer->base = __raw_get_cpu_var(tvec_bases);
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

int del_timer(struct timer_list *timer)
{
	//struct tvec_base *base;
	//unsigned long flags;
	int ret = 0;

	//timer_stats_timer_clear_start_info(timer);
	if (timer_pending(timer)) {
		//base = lock_timer_base(timer, &flags);
		//base = timer->base;
		if (timer_pending(timer)) {
			detach_timer(timer, 1);
			ret = 1;
		}
		//spin_unlock_irqrestore(&base->lock, flags);
	}

	return ret;
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
	//update_times(ticks);
}

void update_process_times(void)
{
	run_local_timers();
	//TODO
	scheduler_tick();
}

/*
 * Called by the local, per-CPU timer interrupt on SMP.
 */
void run_local_timers(void)
{
	//hrtimer_run_queues();
	raise_softfiq(TIMER_SOFTFIQ);
	//softlockup_tick();
}

void init_timers(void)
{
	init_timers_cpu();
	open_softfiq(TIMER_SOFTFIQ, run_timer_softfiq);
}

static void run_timer_softfiq(void)
{
	//DMSG("\n#debug# hello timer!\n");
	struct tvec_base *base = __get_cpu_var(tvec_bases);

	//hrtimer_run_pending();

	if (time_after_eq(jiffies, base->timer_jiffies))
		__run_timers(base);
}


static int cascade(struct tvec_base *base, struct tvec *tv, int index)
{
	/* cascade all the timers from tv up one level */
	struct timer_list *timer, *tmp;
	struct list_head tv_list;

	list_replace_init(tv->vec + index, &tv_list);

	/*
	 * We are removing _all_ timers from the list, so we
	 * don't have to detach them individually.
	 */
	list_for_each_entry_safe(timer, tmp, &tv_list, entry) {
		//BUG_ON(tbase_get_base(timer->base) != base);
		internal_add_timer(base, timer);
	}

	return index;
}

#define INDEX(N) ((base->timer_jiffies >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)


/**
 * __run_timers - run all expired timers (if any) on this CPU.
 * @base: the timer vector to be processed.
 *
 * This function cascades all vectors and executes all expired timer
 * vectors.
 */
static inline void __run_timers(struct tvec_base *base)
{
	struct timer_list *timer;

	//spin_lock_irq(&base->lock);
	while (time_after_eq(jiffies, base->timer_jiffies)) {
		struct list_head work_list;
		struct list_head *head = &work_list;
		int index = base->timer_jiffies & TVR_MASK;

		/*
		 * Cascade timers:
		 */
		if (!index &&
			(!cascade(base, &base->tv2, INDEX(0))) &&
				(!cascade(base, &base->tv3, INDEX(1))) &&
					!cascade(base, &base->tv4, INDEX(2)))
			cascade(base, &base->tv5, INDEX(3));
		++base->timer_jiffies;
		list_replace_init(base->tv1.vec + index, &work_list);
		while (!list_empty(head)) {
			void (*fn)(unsigned long);
			unsigned long data;

			timer = list_first_entry(head, struct timer_list,entry);
			fn = timer->function;
			data = timer->data;

			//timer_stats_account_timer(timer);

			set_running_timer(base, timer);
			detach_timer(timer, 1);
			fn(data);
		}
	}
	set_running_timer(base, NULL);
	//spin_unlock_irq(&base->lock);
}

static int  init_timers_cpu(void)
{
	int j;
	struct tvec_base *base;
	base = &boot_tvec_bases;
	for (j = 0; j < TVN_SIZE; j++) {
		INIT_LIST_HEAD(base->tv5.vec + j);
		INIT_LIST_HEAD(base->tv4.vec + j);
		INIT_LIST_HEAD(base->tv3.vec + j);
		INIT_LIST_HEAD(base->tv2.vec + j);
	}
	for (j = 0; j < TVR_SIZE; j++)
		INIT_LIST_HEAD(base->tv1.vec + j);

	base->timer_jiffies = jiffies;
	return 0;
}

static inline void set_running_timer(struct tvec_base *base,
					struct timer_list *timer)
{
	base->running_timer = timer;
}

void setup_timer(struct timer_list * timer,
				void (*function)(unsigned long),
				unsigned long data,
				unsigned long delay)
{
	init_timer(timer);
	timer->function = function;
	timer->data = data;
	timer->expires = jiffies + delay;
	add_timer(timer);	
}