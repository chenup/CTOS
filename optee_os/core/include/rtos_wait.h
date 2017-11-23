#ifndef RTOS_WAIT_H
#define RTOS_WAIT_H

void wakeup(unsigned long data);
void __wake_up(void);
void pause(void);

#endif