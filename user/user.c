#include "../os.h"

#define DELAY 4000

// 测试自旋锁
#define USE_LOCK
extern struct spinlock lk;
int num = 1;

// 测试软件定时器
struct userdata {
	int counter;
	char *str;
};

struct userdata person = {0, "Jack"};

void timer_func(void *arg)
{
	if (NULL == arg)
		return;

	struct userdata *param = (struct userdata *)arg;

	param->counter++;
	printf("======> TIMEOUT: %s: %d\n", param->str, param->counter);
}


/*
* task0\task1\task5\task2\task3 用来测试多任务调度
* task7\task8 用来测试自旋锁
* task9\task10 用来测试软件定时器
*/
void user_task0(void* param)
{
	uart_puts("Task 0: Created!\n");
	//task_yield();
	//uart_puts("Task 0: I'm back!\n");
	int i = 0;	
	while (1) {
		i++;
		if(i==3){
			task_exit();
		}else{
			printf("param0: %s, i== %d\n", (char*)param, i);
			task_delay(DELAY);
		}
	}
}

void user_task1(void* param)
{
	uart_puts("Task 1: Created!\n");
	int i = 0;	
	while (1) {
		i++;
		if(i==10){
			task_exit();
		}else{
			printf("param1: %s, i== %d\n", (char*)param, i);
			task_delay(DELAY);
		}
	}
}

void user_task5(void* param)
{
	uart_puts("Task 5: Created!\n");
	int i = 0;	
	while (1) {
		i++;
		if(i==50){
			task_exit();
		}else{
			printf("param5: %s, i== %d\n", (char*)param, i);
			task_delay(DELAY);
		}
		
		//task_yield();
	}
}

void user_task2(void* param)
{
	uart_puts("Task 2: Created!\n");
	//printf("param: %s\n", (char*)param);
	int i = 0;	
	while (1) {
		i++;
		if(i==3){
			//task_exit();
		}
		//printf("param2: %s\n", (char*)param);
		//printf("param2: Task 2\n");
		uart_puts("param2: Task 2\n");
		task_delay(DELAY);
		task_yield();
	}
}

void user_task3(void* param)
{
	uart_puts("Task 3: Created!\n");
	//printf("param: %s\n", (char*)param);
	int i = 0;	
	while (1) {
		i++;
		if(i==3){
			//task_exit();
		}
		//printf("param3: %s\n", (char*)param);
		//printf("param3: Task 3\n");
		uart_puts("param3: Task 3\n");
		task_delay(DELAY);
		task_yield();
	}
}

void user_task7(void* param)
{
	uart_puts("Task 7: Created!\n");
	while (1) {
#ifdef USE_LOCK
		//printf("ld addr: 0x%x, locked: %d\n", &lk, lk.locked);
		spin_lock(&lk);
#endif	
		//uart_puts("Task 0: Begin ... \n");
		for (int i = 0; i < 5; i++) {
			//printf("Task 0: num: %d\n", num);
			task_delay(DELAY);
			num = num+1;
			printf("Task 7: num: %d\n", num);
		}
		//uart_puts("Task 0: End ... \n");
#ifdef USE_LOCK
		spin_unlock(&lk);
#endif
	}
}

void user_task8(void* param)
{
	uart_puts("Task 8: Created!\n");
	while (1) {
#ifdef USE_LOCK
		//printf("ld addr: 0x%x, locked: %d\n", &lk, lk.locked);
		spin_lock(&lk);
		
#endif
		//uart_puts("Task 1: Begin ... \n");
		for (int i = 0; i < 5; i++) {
			num = num+1;
			task_delay(DELAY);
			printf("Task 8: num: %d\n", num);
		}
		//uart_puts("Task 1: End ... \n");
#ifdef USE_LOCK
		spin_unlock(&lk);
#endif
	}
}

void user_task9(void* param)
{
	uart_puts("Task 9: Created!\n");

	struct timer *t1 = timer_create(timer_func, &person, 10);
	if (NULL == t1) {
		printf("timer_create() failed!\n");
	}
	struct timer *t2 = timer_create(timer_func, &person, 5);
	if (NULL == t2) {
		printf("timer_create() failed!\n");
	}
	struct timer *t3 = timer_create(timer_func, &person, 5);
	if (NULL == t3) {
		printf("timer_create() failed!\n");
	}
	int i = 1;
	while (1) {
		i++;
		uart_puts("Task 9: Running... \n");
		task_delay(DELAY);
		/*
		if(i == 10){
			timer_delete(t1);
		}
		if(i == 15){
			timer_delete(t3);
		}		
		*/

	}
}

void user_task10(void* param)
{
	uart_puts("Task 10: Created!\n");
	while (1) {
		uart_puts("Task 10: Running... \n");
		task_delay(DELAY);
	}
}

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void)
{
	/*
	// 1. 测试抢占式优先级多任务调度
	char* param0 = "Task 0: priority 0\n";
	task_create_priority(user_task0, param0, 0, 10000000);
	printf("user_task0 == 0x%x\n ", user_task0);
	char* param1 = "Task 1: priority 0\n";
	task_create_priority(user_task1, param1, 0, 20000000);
	char* param5 = "Task 5: priority 0\n";
	task_create_priority(user_task5, param5, 0, 40000000);
	
	char* param2 = "Task 2: priority 1\n";
	task_create_priority(user_task2, param2, 1, 10000000);

	char* param3 = "Task 3: priority 1\n";
	task_create_priority(user_task3, param3, 1, 10000000);	
	*/

	/*
	// 2. 测试自旋锁
	char* param7 = "Task 7: priority 0\n";
	task_create_priority(user_task7, param7, 0, 10000000);
	char* param8 = "Task 8: priority 0\n";
	task_create_priority(user_task8, param8, 0, 10000000);
	*/

	///*
	// 3. 测试软件定时器
	char* param9 = "Task 9: priority 0\n";
	task_create_priority(user_task9, param9, 0, 10000000);
	char* param10 = "Task 10: priority 0\n";
	task_create_priority(user_task10, param10, 0, 20000000);
	//*/
	

}

