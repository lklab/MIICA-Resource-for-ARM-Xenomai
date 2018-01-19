#include "os.h"

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <native/task.h>
#include <native/timer.h>

static os_sig_t registered_handler = NULL;

static void rt_task_proc(void *arg);
static void set_rt_task_timer(RT_TASK* rt_task_plc, unsigned long long next, unsigned long long period);
static void sigint_handler(int sig);

int os_task_init(os_task_t* task, os_proc_t proc, unsigned long long period)
{
	RT_TASK* rt_task_plc;
	
    mlockall(MCL_CURRENT | MCL_FUTURE);

	task -> data = NULL;
	rt_task_plc = (RT_TASK*)malloc(sizeof(RT_TASK));
	if(rt_task_plc == NULL)
		return 1;

	if(rt_task_create(rt_task_plc, "rt_task_plc", 0, 50, T_JOINABLE))
	{
		free(rt_task_plc);
		return 1;
	}

	task -> proc = proc;
	task -> period = period;
	task -> alive = 0;
	task -> data = (void*)rt_task_plc;

	return 0;
}

int os_task_start(os_task_t* task)
{
	if(task -> alive)
		return 1;
	if(task -> data == NULL)
		return 1;

	task -> alive = 1;
	if(rt_task_start((RT_TASK*)(task -> data), &rt_task_proc, task))
		return 1;
	rt_task_join((RT_TASK*)(task -> data));

	return 0;
}

int os_task_stop(os_task_t* task)
{
	task -> alive = 0;

	if(task -> data != NULL)
	{
		rt_task_join((RT_TASK*)(task -> data));
		rt_task_delete((RT_TASK*)(task -> data));

		free(task -> data);
		task -> data = NULL;
	}

	return 0;
}

int os_signal(os_sig_t handler)
{
	registered_handler = handler;
	signal(SIGINT, sigint_handler);
	return 0;
}

void os_exit(int value)
{
	exit(value);
}

void* os_memcpy(void* s1, const void* s2, unsigned int n)
{
	return memcpy(s1, s2, (size_t)n);
}

static void rt_task_proc(void *arg)
{
	os_task_t* task = (os_task_t*)arg;

	set_rt_task_timer((RT_TASK*)(task -> data), task -> period, task -> period);

	while(task -> alive)
	{
		task -> proc();
		rt_task_wait_period(NULL);
	}
}

void set_rt_task_timer(RT_TASK* rt_task_plc, unsigned long long next, unsigned long long period)
{
	RTIME current_time = rt_timer_read();
	rt_task_set_periodic(rt_task_plc, current_time + next, rt_timer_ns2ticks(period));
}

static void sigint_handler(int sig)
{
	if(registered_handler != NULL)
		registered_handler();
}
