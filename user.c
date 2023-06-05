#include "os.h"

#define DELAY 1000

void user_task0(void* param)
{
	int i = 0;	
	while (1) {
		i++;
		if(i==3){
			task_exit();
		}
		printf("param0: %s\n", (char*)param);
		//printf("param0: Task 0\n");
		task_delay(DELAY);
		task_yield();
	}
	
	
	//task_exit();
}

void user_task1(void* param)
{
	
	int i = 0;	
	while (1) {
		i++;
		if(i==10){
			task_exit();
		}
		printf("param1: %s\n", (char*)param);
		//printf("param1: Task 1\n");
		task_delay(DELAY);
		task_yield();
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

/* NOTICE: DON'T LOOP INFINITELY IN main() */
void os_main(void)
{
	/*task_create(user_task0);
	task_create(user_task1);
	task_create(user_task2);
	task_create(user_task3);
	*/
	char* param0 = "Task 0: priority 0\n";
	task_create_priority(user_task0, param0, 0);
	printf("user_task0 == 0x%x\n ", user_task0);
	char* param1 = "Task 1: priority 0\n";
	task_create_priority(user_task1, param1, 0);
	
	char* param2 = "Task 2: priority 0\n";
	task_create_priority(user_task2, param2, 1);

	char* param3 = "Task 3: priority 0\n";
	task_create_priority(user_task3, param3, 1);	
	
}

