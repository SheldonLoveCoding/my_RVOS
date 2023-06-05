#include "os.h"

/* defined in entry.S */
extern void switch_to(struct context *next);

#define MAX_TASKS 10
#define STACK_SIZE 1024
#define MAX_PRIORITY 256

uint8_t task_stack[MAX_TASKS][STACK_SIZE];
struct context ctx_tasks[MAX_TASKS];

TaskNode tasks_priority[MAX_PRIORITY][2]; //优先级数组，用来保存每一个优先级的任务链表的首尾
uint8_t tasks_num[MAX_PRIORITY]; //每一个优先级中任务的数量
uint8_t task_stack_priority[MAX_PRIORITY][MAX_TASKS][STACK_SIZE];//对应优先级的任务栈空间

/* 利用mscratch寄存器切换上下文，这句汇编代表对mscratch寄存器写 */
static void w_mscratch(reg_t x)
{
	asm volatile("csrw mscratch, %0" : : "r" (x));
}

void sched_init()
{
	w_mscratch(0);

	for(int i_pri = 0;i_pri < MAX_PRIORITY;i_pri++){
		tasks_priority[i_pri][0].next = &tasks_priority[i_pri][1];
		tasks_priority[i_pri][0].pre = NULL;
		tasks_priority[i_pri][1].next = NULL;
		tasks_priority[i_pri][1].pre = &tasks_priority[i_pri][0];
	}

}

/*
 * 实现基于优先级的FIFO任务调度算法
 */

void schedule_priority()
{
	
	//确定优先级
	int priority = 0;
	while(priority < MAX_PRIORITY){
		if(tasks_num[priority] > 0){
			break;
		}
		priority++;
	}
	//记录下要调用的下一个任务
	struct context *next = tasks_priority[priority][0].next->task;
	TaskNode * next_node = tasks_priority[priority][0].next;
	//将该任务移到链表末尾
	datch_taskNode(next_node);
	add_taskNode(&tasks_priority[priority][0], &tasks_priority[priority][1], next_node, priority);
	//跳转
	switch_to(next);
	
}

//将任务节点添加到任务链表尾部
int add_taskNode(TaskNode* first, TaskNode* tail, TaskNode* task_new_node, int priority){
	if (tasks_num[priority] < MAX_TASKS){
		task_new_node->pre = tail->pre;
		task_new_node->next = tail;
		tail->pre->next = task_new_node;
		tail->pre = task_new_node;		
		return 0;
	}else{
		printf("超出最大任务数\n");
		return -1;
	}
}
//将任务节点在链表中删除(datch)
int datch_taskNode(TaskNode* task_node){
	task_node->pre->next = task_node->next;
	task_node->next->pre = task_node->pre;
	task_node->next = NULL;
	task_node->pre = NULL;
	return 0;
}
/*
 * DESCRIPTION
 * 	创建带有优先级的任务.
 * 	- start_routin: 任务入口
 * RETURN VALUE
 * 	0: 成功
 * 	-1: 出错
 */
int task_create_priority(void (*start_routin)(void* param), void* param, int priority)
{
	//判断任务数量是否超过最大数目
	if (tasks_num[priority] < MAX_TASKS) {
		//创建上下文
		struct context* ctx_task = (struct context*)my_malloc(sizeof(struct context));
		ctx_task->sp = (reg_t) &task_stack_priority[priority][tasks_num[priority]][STACK_SIZE - 1];
		ctx_task->ra = (reg_t) start_routin;
		ctx_task->a0 = (reg_t) param;
		//创建任务节点
		TaskNode* task_new_node = (TaskNode*)my_malloc(sizeof(TaskNode));
		task_new_node->task = ctx_task;

		//加入到对应优先级的任务链表
		add_taskNode(&tasks_priority[priority][0], &tasks_priority[priority][1], task_new_node, priority);
		tasks_num[priority]++;
		return 0;
	} else {
		return -1;
	}
}



void task_exit()
{
	//既然当前任务在调用这函数，则说明该任务优先级最高
	//确定优先级
	int priority = 0;
	while(priority < MAX_PRIORITY){
		if(tasks_num[priority] > 0){
			break;
		}
		priority++;
	}

	//首先记录下当前调用的任务
	TaskNode * cur_node = tasks_priority[priority][1].pre;
	//将该任务节点脱离链表
	datch_taskNode(cur_node);
	//释放该节点的内存
	my_free(cur_node);
	tasks_num[priority]--;
	return;
}



/*
 * DESCRIPTION
 * 	task_yield()  causes the calling task to relinquish the CPU and a new 
 * 	task gets to run.
 *  协作式多任务，主动交出CPU控制权
 */
void task_yield()
{
	
	schedule_priority();
}

/*
 * a very rough implementaion, just to consume the cpu
 */
void task_delay(volatile int count)
{
	count *= 50000;
	while (count--);
}

