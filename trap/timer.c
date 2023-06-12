#include "../os.h"
/* interval ~= 1s */
#define TIMER_INTERVAL CLINT_TIMEBASE_FREQ
#define MAX_TIMER 10
static uint32_t _tick = 0;

extern void schedule_priority(void);
extern TaskNode* task_global_ptr;

extern struct spinlock lk;

static struct timer timer_list[MAX_TIMER];
struct TimerNode dummyHead;

/* load timer interval(in ticks) for next timer interrupt.*/
void timer_load(int interval)
{
	/* each CPU has a separate source of timer interrupts. */
	int id = r_mhartid();
	
	*(uint64_t*)CLINT_MTIMECMP(id) = *(uint64_t*)CLINT_MTIME + interval;
}


// 定时器初始化：1. 初始化软件定时器数组 2. 初始化mtimecmp 3. 开启定时器中断mie
void timer_init()
{
	//
	/*
	struct timer *t = &(timer_list[0]);
	for (int i = 0; i < MAX_TIMER; i++) {
		t->func = NULL; // 用数组管理时，用func是否为NULL来判断是否有定时器任务
		t->arg = NULL;
		t++;
	}	
	*/
	struct timer* t = (struct timer *)my_malloc(sizeof(struct timer));
	t->func = NULL;
	t->arg = NULL;
	t->timeout_tick = 0;
	dummyHead.timer = t;
	dummyHead.next = NULL;
	

	/*
	 * On reset, mtime is cleared to zero, but the mtimecmp registers 
	 * are not reset. So we have to init the mtimecmp manually.
	 */
	timer_load(TIMER_INTERVAL);

	/* enable machine-mode timer interrupts. */
	w_mie(r_mie() | MIE_MTIE);
}

// 添加 timeNode
int add_TimeNode(struct TimerNode* dummyHead, struct TimerNode* node)
{
	if(dummyHead->next == NULL){
		dummyHead->next = node;
		return 0;
	}
	struct TimerNode* pre = dummyHead;
	struct TimerNode* cur = dummyHead->next;
	while(cur){
		if(cur->timer->timeout_tick >= node->timer->timeout_tick){
			node->next = cur;
			pre->next = node;
			return 0;
		}
		pre = cur;
		cur = cur->next;
	}
	if(cur == NULL && pre->timer->timeout_tick < node->timer->timeout_tick){
		pre->next = node;
	}
	return 0;
}

// 创建软件定时器，超时处理函数、函数参数、超时时间
struct timer *timer_create(void (*handler)(void *arg), void *arg, uint32_t timeout)
{
	/* TBD: params should be checked more, but now we just simplify this */
	if (NULL == handler || 0 == timeout) {
		return NULL;
	}

	/* use lock to protect the shared timer_list between multiple tasks */
	//上锁 保护timer_list
	spin_lock(&lk);	

	// 设置软件定时器
	struct timer* t = (struct timer *)my_malloc(sizeof(struct timer));
	t->func = handler;
	t->arg = arg;
	t->timeout_tick = _tick + timeout;
	struct TimerNode* tn = (struct TimerNode*)my_malloc(sizeof(struct TimerNode));
	tn->timer = t;
	tn->next = NULL;

	add_TimeNode(&dummyHead, tn);

	// 解锁
	spin_unlock(&lk);

	return t;
}

// 删除定时器
void timer_delete(struct timer *timer)
{
	// 上锁
	spin_lock(&lk);
	// 删除定时器
	struct TimerNode* cur = dummyHead.next;
	struct TimerNode* pre = &dummyHead;
	while(cur){
		if(cur->timer == timer){
			pre->next = cur->next;
			cur->next = NULL;
			my_free(cur->timer);
			my_free(cur);
			break;
		}
		pre = cur;
		cur = cur->next;
	}

	// 解锁
	spin_unlock(&lk);
}

//有序链表的二分法来找到超时定时器的右边界
struct TimerNode* find_TimeNode_rb(struct TimerNode* left, struct TimerNode* right, uint32_t _tick)
{
	
}

/* this routine should be called in interrupt context (interrupt is disabled) */
// 检查任务是否超时
static inline void timer_check()
{
	struct TimerNode* cur = dummyHead.next;
	struct TimerNode* pre = &dummyHead;
	struct TimerNode* temp;
	while(cur){
		if(cur->timer->timeout_tick > _tick){
			break; //找到了第一个不超时的任务
		}
		pre = cur;
		cur = cur->next;
	}
	printf("pre->timer->timeout_tick: %d\n", pre->timer->timeout_tick);
	if(pre != &dummyHead){
		cur = dummyHead.next;
		while(cur != pre->next){
			if(cur->timer->func != NULL){
				printf("cur->timer->timeout_tick: %d\n", cur->timer->timeout_tick);
				cur->timer->func(cur->timer->arg);
				cur->timer->func = NULL;
				//temp = cur;
				
				//timer_delete(temp->timer);
			}
			cur = cur->next;
		}
	}

	
}

void timer_handler() 
{
	_tick++;
	printf("task_id: %d, time_slice: %d\n", task_global_ptr->task_id, task_global_ptr->timeslice);
	//printf("task_timeslice_0: %d, task_timeslice_1: %d\n", task_timeslice[0], task_timeslice[1]);
	timer_check();
	/*
	struct TimerNode* cur = dummyHead.next;
	while(cur){
		printf("cur->timer: 0x%x, timeout: %d\n", cur->timer, cur->timer->timeout_tick);
		cur = cur->next;
	}		
	*/


	timer_load(task_global_ptr->timeslice);

	schedule_priority();
}
