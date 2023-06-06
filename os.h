#ifndef __OS_H__
#define __OS_H__

#include "types.h"
#include "platform.h"
#include "riscv.h"

#include <stddef.h>
#include <stdarg.h>

#define MAX_TASKS 10
#define STACK_SIZE 1024
#define MAX_PRIORITY 256

/* uart */
extern int uart_putc(char ch);
extern void uart_puts(char *s);

/* printf */
extern int  printf(const char* s, ...);
extern void panic(char *s);

/* memory management */
extern void *page_alloc(int npages);
extern void page_free(void *p);

extern void *my_malloc(size_t size); 
extern void my_free(void *ptr);

/* task management */
struct context {
	/* ignore x0 */
	reg_t ra;
	reg_t sp;
	reg_t gp;
	reg_t tp;
	reg_t t0;
	reg_t t1;
	reg_t t2;
	reg_t s0;
	reg_t s1;
	reg_t a0;
	reg_t a1;
	reg_t a2;
	reg_t a3;
	reg_t a4;
	reg_t a5;
	reg_t a6;
	reg_t a7;
	reg_t s2;
	reg_t s3;
	reg_t s4;
	reg_t s5;
	reg_t s6;
	reg_t s7;
	reg_t s8;
	reg_t s9;
	reg_t s10;
	reg_t s11;
	reg_t t3;
	reg_t t4;
	reg_t t5;
	reg_t t6;
	// upon is trap frame

	// save the pc to run in next schedule cycle
	reg_t pc; // offset: 31 *4 = 124
};

// 双向任务节点
typedef struct taskNode{
	struct context* task;
	uint32_t timeslice;
	uint32_t task_id;
	struct taskNode* pre;
	struct taskNode* next;
}TaskNode;

extern void task_delay(volatile int count);
extern void task_yield();

//优先级任务管理
extern int task_create_priority(void (*start_routin)(void* param), void* param, int priority, uint32_t timeslice);
extern int add_taskNode(TaskNode* first, TaskNode* tail, TaskNode* task_new_node, int priority);
extern int datch_taskNode(TaskNode* task_node);
extern void task_exit(void);

/* plic */
extern int plic_claim(void);
extern void plic_complete(int irq);

#endif /* __OS_H__ */
