#ifndef RTOS_JIFFIES_H
#define RTOS_JIFFIES_H
#include <stdint.h>
#include <compiler.h>
#define __jiffy_data  __section(".data")
#define HZ 1000
#define INITIAL_JIFFIES ((unsigned long)(unsigned int) (-300*HZ))

typedef uint64_t u64
extern u64 __jiffy_data jiffies_64;

#define time_after_eq(a,b)	\
	 ((long)(a) - (long)(b) >= 0)
#endif
