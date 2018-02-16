#ifndef RTOS_TIME_H
#define RTOS_TIME_H

void timer_trick(void);
void do_timer(unsigned long ticks);
void update_wall_time(void);
void update_process_times(void);
//TODO 2018-2-16
uint64_t get_monotonic(void);
#endif