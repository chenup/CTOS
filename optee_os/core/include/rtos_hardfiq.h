#ifndef RTOS_HARDFIQ_H
#define RTOS_HARDFIQ_H


extern void fiq_enter(void);
extern void fiq_exit(void);
extern int get_fiq_count(void);
//extern void add_fiq_count(void);
//extern void sub_fiq_count(void);

typedef struct {
	unsigned int __softfiq_pending;
} fiq_cpustat_t;

//simple
fiq_cpustat_t fiq_stat[8];
//simple
int fiq_count;

#define in_interrupt()	get_fiq_count()

#define add_fiq_count()					\
	do {						\
		fiq_count++;	\
	} while (0)

#define sub_fiq_count()					\
	do {						\
		fiq_count--;	\
	} while (0)	


#endif