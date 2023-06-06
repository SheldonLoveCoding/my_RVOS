#include "os.h"

extern void trap_vector(void);
extern void uart_isr(void);
extern void timer_handler(void);
extern void schedule_priority(void);


void trap_init()
{
	/*
	 * set the trap-vector base-address for machine-mode
	 */
	// 设置trap发生时，程序需要跳转的地址。mtvec寄存器
	// trap_vector实现在了 entry.S 中
	// tarp_vector的作用是保存上下文，然后跳转到trap处理函数 trap_handler 中
	w_mtvec((reg_t)trap_vector);
}

// 外部中断处理函数
void external_interrupt_handler()
{
	int irq = plic_claim(); // 获得目前发生的最高优先级的中断源

	if (irq == UART0_IRQ){ // 如果是UART0的中断
      	uart_isr(); // 处理UART0中断的逻辑
	} else if (irq) {
		printf("unexpected interrupt irq = %d\n", irq);
	}
	
	if (irq) {
		plic_complete(irq); // 告知 PLIC 响应完成
	}
}

/*
* 这相当于trap的解析函数，来判断具体是哪种中断
* trap_handler利用mepc和mcause寄存器的值来确定trap的类型（中断或异常）
* 中断 Asynchronous trap 程序返回到原来指令的下一条指令地址
* 异常 Synchronous trap  程序返回到原来指令的地址
*/

reg_t trap_handler(reg_t epc, reg_t cause)
{
	reg_t return_pc = epc;
	reg_t cause_code = cause & 0xfff;
	
	if (cause & 0x80000000) {
		/* Asynchronous trap - interrupt */
		switch (cause_code) {
		case 3:
			uart_puts("software interruption!\n");
			/*
			 * acknowledge the software interrupt by clearing
    			 * the MSIP bit in mip.
			 */
			int id = r_mhartid();
    		*(uint32_t*)CLINT_MSIP(id) = 0;

			schedule_priority();

			break;
		case 7:
			uart_puts("timer interruption!\n");
			timer_handler();
			break;
		case 11:
			uart_puts("external interruption!\n");
			external_interrupt_handler();
			break;
		default:
			uart_puts("unknown async exception!\n");
			break;
		}
	} else {
		/* Synchronous trap - exception */
		printf("Sync exceptions!, code = %d\n", cause_code);
		panic("OOPS! What can I do!");
		//return_pc += 4;
	}

	return return_pc;
}

void trap_test()
{
	/*
	 * Synchronous exception code = 7
	 * Store/AMO access fault
	 */
	*(int *)0x00000000 = 100;

	/*
	 * Synchronous exception code = 5
	 * Load access fault
	 */
	//int a = *(int *)0x00000000;

	uart_puts("Yeah! I'm return back from trap!\n");
}

