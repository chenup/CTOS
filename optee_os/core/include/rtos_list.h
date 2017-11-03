#ifndef RTOS_LIST_H
#define RTOS_LIST_H

struct list_head {
	struct list_head *next, *prev;
};

#endif