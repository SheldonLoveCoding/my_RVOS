#include "../os.h"

void lock_init(struct spinlock* lk){
	lk->locked = 0;
	
	return;
}

int spin_lock(struct spinlock* lk)
{
	//w_mstatus(r_mstatus() & ~MSTATUS_MIE);
	
	while(__sync_lock_test_and_set(&(lk->locked), 1) != 0);

	return 0;
}

int spin_unlock(struct spinlock* lk)
{
	//w_mstatus(r_mstatus() | MSTATUS_MIE);
	lk->locked = 0;
	return 0;
}
