#ifndef RTOS_PERCPU_H
#define RTOS_PERCPU_H

#define __get_cpu_var(var)			per_cpu_var(var)
#define __raw_get_cpu_var(var)			per_cpu_var(var)

#define per_cpu_var(var) per_cpu__##var
#endif