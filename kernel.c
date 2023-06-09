#include "os.h"

/*
 * Following functions SHOULD be called ONLY ONE time here,
 * so just declared here ONCE and NOT included in file os.h.
 */
extern void uart_init(void);
extern void page_init(void);
extern void malloc_init(void);
extern void trap_init(void);
extern void plic_init(void);
extern void timer_init(void);
extern void sched_init(void);
extern void schedule_priority(void);
extern void os_main(void);

extern int lock_init(struct spinlock* lk);
struct spinlock lk;

void start_kernel(void)
{
	uart_init();
	uart_puts("Hello, RVOS!\n");

	malloc_init();

	trap_init();

	plic_init();
	uart_puts("timplic_initer is done!\n");
	timer_init();
	uart_puts("timer is done!\n");
	sched_init();

	os_main();
	uart_puts("task create is done!\n");
	
	schedule_priority();
	
	uart_puts("Would not go here!\n");
	
	while (1) {}; // stop here!
}

