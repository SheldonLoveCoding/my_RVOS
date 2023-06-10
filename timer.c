#include "os.h"
/* interval ~= 1s */
#define TIMER_INTERVAL CLINT_TIMEBASE_FREQ

extern void schedule_priority(void);
static uint32_t _tick = 0;

extern TaskNode* task_global_ptr;


/* load timer interval(in ticks) for next timer interrupt.*/
void timer_load(int interval)
{
	/* each CPU has a separate source of timer interrupts. */
	int id = r_mhartid();
	
	*(uint64_t*)CLINT_MTIMECMP(id) = *(uint64_t*)CLINT_MTIME + interval;
}

void timer_init()
{
	/*
	 * On reset, mtime is cleared to zero, but the mtimecmp registers 
	 * are not reset. So we have to init the mtimecmp manually.
	 */
	timer_load(TIMER_INTERVAL);

	/* enable machine-mode timer interrupts. */
	w_mie(r_mie() | MIE_MTIE);
}

void timer_handler() 
{
	_tick++;
	printf("task_id: %d, time_slice: %d\n", task_global_ptr->task_id, task_global_ptr->timeslice);
	//printf("task_timeslice_0: %d, task_timeslice_1: %d\n", task_timeslice[0], task_timeslice[1]);

	timer_load(task_global_ptr->timeslice);

	schedule_priority();
}
