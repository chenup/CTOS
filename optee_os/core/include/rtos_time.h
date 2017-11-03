#ifndef RTOS_TIME_H
#define RTOS_TIME_H

void timer_trick(void);
void do_timer(unsigned long ticks);
extern void update_wall_time(void);

#endif