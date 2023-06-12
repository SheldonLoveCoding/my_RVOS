#include "../os.h"

void plic_init(void)
{
	int hart = r_tp(); // 读取 tp寄存器的值。tp寄存器 是指 thread pointer
  
	/* 
	 * Set priority for UART0.
	 *
	 * Each PLIC interrupt source can be assigned a priority by writing 
	 * to its 32-bit memory-mapped priority register.
	 * The QEMU-virt (the same as FU540-C000) supports 7 levels of priority. 
	 * A priority value of 0 is reserved to mean "never interrupt" and 
	 * effectively disables the interrupt. 
	 * Priority 1 is the lowest active priority, and priority 7 is the highest. 
	 * Ties between global interrupts of the same priority are broken by 
	 * the Interrupt ID; interrupts with the lowest ID have the highest 
	 * effective priority.
	 */
	// #define PLIC_PRIORITY(id) (PLIC_BASE + (id) * 4)，设置PLIC的priotity寄存器
	// 用来指定中断源的优先级
	// UART0_IRQ 是UART的中断源，在这里是由qemu决定的
	*(uint32_t*)PLIC_PRIORITY(UART0_IRQ) = 1;
 
	/*
	 * Enable UART0
	 *
	 * Each global interrupt can be enabled by setting the corresponding 
	 * bit in the enables registers.
	 */
	// 针对该 hart 使能 UART0 的中断源
	*(uint32_t*)PLIC_MENABLE(hart)= (1 << UART0_IRQ);

	/* 
	 * Set priority threshold for UART0.
	 *
	 * PLIC will mask all interrupts of a priority less than or equal to threshold.
	 * Maximum threshold is 7.
	 * For example, a threshold value of zero permits all interrupts with
	 * non-zero priority, whereas a value of 7 masks all interrupts.
	 * Notice, the threshold is global for PLIC, not for each interrupt source.
	 */
	// 设置该 hart 的中断优先级的阈值，优先级小于该阈值的不响应
	*(uint32_t*)PLIC_MTHRESHOLD(hart) = 0;

	/* enable machine-mode global interrupts. */
	// 使能machine模式下的全局中断
	//w_mstatus(r_mstatus() | MSTATUS_MIE);

	/* enable machine-mode external interrupts. */
	// 使能machine模式下的外部中断
	w_mie(r_mie() | MIE_MEIE);

}

/* 
 * DESCRIPTION:
 *	Query the PLIC what interrupt we should serve.
 *	Perform an interrupt claim by reading the claim register, which
 *	returns the ID of the highest-priority pending interrupt or zero if there 
 *	is no pending interrupt. 
 *	A successful claim also atomically clears the corresponding pending bit
 *	on the interrupt source.
 * RETURN VALUE:
 *	the ID of the highest-priority pending interrupt or zero if there 
 *	is no pending interrupt.
 */

// 获得等待响应的中断源中 最高优先级的中断源，并将对应的pending位置零
int plic_claim(void)
{
	int hart = r_tp();
	int irq = *(uint32_t*)PLIC_MCLAIM(hart); // 读 PLIC 的 claim 寄存器
	return irq;
}

/* 
 * DESCRIPTION:
  *	Writing the interrupt ID it received from the claim (irq) to the 
 *	complete register would signal the PLIC we've served this IRQ. 
 *	The PLIC does not check whether the completion ID is the same as the 
 *	last claim ID for that target. If the completion ID does not match an 
 *	interrupt source that is currently enabled for the target, the completion
 *	is silently ignored.
 * RETURN VALUE: none
 */
// irq中断响应完成之后，通过向PLIC的complete寄存器的写操作，以告知PLIC该中断已响应结束
void plic_complete(int irq)
{
	int hart = r_tp();
	*(uint32_t*)PLIC_MCOMPLETE(hart) = irq;
}
