#ifndef RTOS_TIMER_H
#define RTOS_TIMER_H

#include <rtos_list.h>

#define TVN_BITS 6
#define TVR_BITS 8
#define TVN_SIZE (1 << TVN_BITS)
#define TVR_SIZE (1 << TVR_BITS)
#define TVN_MASK (TVN_SIZE - 1)
#define TVR_MASK (TVR_SIZE - 1)

#if !defined(NULL)
#define NULL (void *)0
#endif
#define TRUE    1

#ifndef bool
#define bool int
#endif

#ifndef false
#define false 0
#endif

struct tvec_base;

struct timer_list {
	struct list_head entry;
	unsigned long expires;

	void (*function)(unsigned long);
	unsigned long data;

	struct tvec_base *base;
};

extern struct tvec_base boot_tvec_bases;

static inline int timer_pending(const struct timer_list * timer)
{
	return timer->entry.next != NULL;
}

extern void add_timer(struct timer_list *timer);
extern int mod_timer(struct timer_list *timer, unsigned long expires);
extern int del_timer(struct timer_list * timer);
extern void run_local_timers(void);
extern void init_timer(struct timer_list *timer);
extern void init_timers(void);
extern void setup_timer(struct timer_list * timer, void (*function)(unsigned long), unsigned long data, unsigned long delay);
/*
 * Magic number "tsta" to indicate a static timer initializer
 * for the object debugging code.
 */
#define TIMER_ENTRY_STATIC	((void *) 0x74737461)

#define TIMER_INITIALIZER(_function, _expires, _data) {		\
		.entry = { .prev = TIMER_ENTRY_STATIC },	\
		.function = (_function),			\
		.expires = (_expires),				\
		.data = (_data),				\
		.base = &boot_tvec_bases			}

#define DEFINE_TIMER(_name, _function, _expires, _data)		\
	struct timer_list _name =				\
		TIMER_INITIALIZER(_function, _expires, _data)

#endif