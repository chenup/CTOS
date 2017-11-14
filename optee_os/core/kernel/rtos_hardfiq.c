#include <rtos_hardfiq.h>

int get_fiq_count(void)
{
	return fiq_count;
}

/*
void add_fiq_count(void)
{
	fiq_count++;
}

void sub_fiq_count(void)
{
	fiq_count--;
}
*/
