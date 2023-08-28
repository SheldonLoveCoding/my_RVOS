#include "../os.h"

/*
 * Following global vars are defined in mem.S
 */
extern uint32_t TEXT_START;
extern uint32_t TEXT_END;
extern uint32_t DATA_START;
extern uint32_t DATA_END;
extern uint32_t RODATA_START;
extern uint32_t RODATA_END;
extern uint32_t BSS_START;
extern uint32_t BSS_END;
extern uint32_t HEAP_START;
extern uint32_t HEAP_SIZE;

/*
 * _alloc_start points to the actual start address of heap pool
 * _alloc_end points to the actual end address of heap pool
 * _num_pages holds the actual max number of pages we can allocate.
 */
static uint32_t _alloc_start = 0;
static uint32_t _alloc_end = 0;
static uint32_t _num_pages = 0;

static uint32_t _allocd_page_end = 0;
static uint32_t _mlloc_initialized = 0;     // 初始化malloc标志
void *managed_memory_start;  // 指向堆底（内存块起始位置）
void *last_valid_address;    // 指向堆顶

/* 内存控制块，用来描述malloc的开辟的nbytes内存块的信息*/
struct mem_control_block {
  
  uint32_t size;         // 实际空间的大小
  uint8_t is_used; // 是否可用（如果还没被分配出去，就是 0）
};

#define PAGE_SIZE 4096
#define PAGE_ORDER 12

#define PAGE_TAKEN (uint8_t)(1 << 0)
#define PAGE_LAST  (uint8_t)(1 << 1)

/*
 * Page Descriptor 
 * flags:
 * - bit 0: flag if this page is taken(allocated)
 * - bit 1: flag if this page is the last page of the memory block allocated
 */
struct Page {
	uint8_t flags;
};

static inline void _clear(struct Page *page)
{
	page->flags = 0;
}

static inline int _is_free(struct Page *page)
{
	if (page->flags & PAGE_TAKEN) {
		return 0;
	} else {
		return 1;
	}
}

static inline void _set_flag(struct Page *page, uint8_t flags)
{
	page->flags |= flags;
}

static inline int _is_last(struct Page *page)
{
	if (page->flags & PAGE_LAST) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * align the address to the border of page(4K)
 */
static inline uint32_t _align_page(uint32_t address)
{
	uint32_t order = (1 << PAGE_ORDER) - 1;
	return (address + order) & (~order);
}

void page_init()
{
	/* 
	 * We reserved 8 Page (8 x 4096) to hold the Page structures.
	 * It should be enough to manage at most 128 MB (8 x 4096 x 4096) 
	 */
	_num_pages = (HEAP_SIZE / PAGE_SIZE) - 8;
	printf("HEAP_START = %x, HEAP_SIZE = %x, num of pages = %d\n", HEAP_START, HEAP_SIZE, _num_pages);
	
	struct Page *page = (struct Page *)HEAP_START;
	for (int i = 0; i < _num_pages; i++) {
		_clear(page);
		page++;	
	}

	_alloc_start = _align_page(HEAP_START + 8 * PAGE_SIZE);
	_alloc_end = _alloc_start + (PAGE_SIZE * _num_pages);

	printf("TEXT:   0x%x -> 0x%x\n", TEXT_START, TEXT_END);
	printf("RODATA: 0x%x -> 0x%x\n", RODATA_START, RODATA_END);
	printf("DATA:   0x%x -> 0x%x\n", DATA_START, DATA_END);
	printf("BSS:    0x%x -> 0x%x\n", BSS_START, BSS_END);
	printf("HEAP:   0x%x -> 0x%x\n", _alloc_start, _alloc_end);
}

/*
 * Allocate a memory block which is composed of contiguous physical pages
 * - npages: the number of PAGE_SIZE pages to allocate
 */
void *page_alloc(int npages)
{
	/* Note we are searching the page descriptor bitmaps. */
	int found = 0;
	struct Page *page_i = (struct Page *)HEAP_START;
	for (int i = 0; i <= (_num_pages - npages); i++) {
		if (_is_free(page_i)) {
			found = 1;
			/* 
			 * meet a free page, continue to check if following
			 * (npages - 1) pages are also unallocated.
			 */
			struct Page *page_j = page_i + 1;
			for (int j = i + 1; j < (i + npages); j++) {
				if (!_is_free(page_j)) {
					found = 0;
					break;
				}
				page_j++;
			}
			/*
			 * get a memory block which is good enough for us,
			 * take housekeeping, then return the actual start
			 * address of the first page of this memory block
			 */
			if (found) {
				struct Page *page_k = page_i;
				for (int k = i; k < (i + npages); k++) {
					_set_flag(page_k, PAGE_TAKEN);
					page_k++;
				}
				page_k--;
				_set_flag(page_k, PAGE_LAST);
				return (void *)(_alloc_start + i * PAGE_SIZE);
			}
		}
		page_i++;
	}
	return NULL;
}

/*
 * Free the memory block
 * - p: start address of the memory block
 */
void page_free(void *p)
{
	/*
	 * Assert (TBD) if p is invalid
	 */
	if (!p || (uint32_t)p >= _alloc_end) {
		return;
	}
	/* get the first page descriptor of this memory block */
	struct Page *page = (struct Page *)HEAP_START;
	page += ((uint32_t)p - _alloc_start)/ PAGE_SIZE;
	/* loop and clear all the page descriptors of the memory block */
	while (!_is_free(page)) {
		if (_is_last(page)) {
			_clear(page);
			break;
		} else {
			_clear(page);
			page++;;
		}
	}
}

/*
 * 字节为单位的malloc的初始化
 */
void malloc_init() {
	// 首先要进行page的初始化，得到_alloc_start 和 _alloc_end
	page_init();
	// 这里不向操作系统申请堆空间，只是为了获取堆的起始地址和最后有效地址
	last_valid_address = (void *)_alloc_end; // 堆的最后有效地址last_valid_address
	managed_memory_start = (void *)_alloc_start; //堆的起始地址managed_memory_start
	
	_mlloc_initialized = 1;
}

void *my_malloc(size_t numbytes) {
	char *current_location;  // 当前访问的内存位置
	char *current_location_tail;  // 当前访问的内存块的末尾位置
	struct mem_control_block *current_location_mcb;  // 只是作了一个强制类型转换
	struct mem_control_block *current_location_mcb_tail;
	
	void *memory_location;  // 这是要返回的内存位置。初始时设为
							// 0，表示没有找到合适的位置
	if (!_mlloc_initialized) {
		malloc_init();
	}
	// 要查找的内存必须包含内存控制块，所以需要调整 numbytes 的大小
	numbytes = numbytes + 2 * sizeof(struct mem_control_block);
	// 初始时设为 0，表示没有找到合适的位置
	memory_location = 0;
	/* Begin searching at the start of managed memory */
	// 从被管理内存的起始位置开始搜索
	// managed_memory_start 是在 malloc_init 中通过 sbrk() 函数设置的
	current_location = managed_memory_start;
	while (current_location < last_valid_address) {
		// current_location 是一个 void 指针，用来计算地址；
		// current_location_mcb 是一个具体的结构体类型
		// 这两个实际上是一个含义
		current_location_mcb = (struct mem_control_block *)current_location;
		
		if (!current_location_mcb->is_used) {
			if((current_location_mcb->size == 0) || (current_location_mcb->size != 0 && current_location_mcb->size >= numbytes))
			{
				// 找到一个新的内存块
				/*
				if((current_location_mcb->size != 0 && current_location_mcb->size >= numbytes)){
					char *_tail = current_location + current_location_mcb->size - sizeof(struct mem_control_block);
					struct mem_control_block * _mcb_tail = (struct mem_control_block *)_tail;
					_mcb_tail->is_used = 0;
					_mcb_tail->size = 0;
				}
				*/
								
				

				current_location_mcb->is_used = 1;  // 设为不可用
				current_location_mcb->size = numbytes;

				memory_location = current_location;      // 设置内存地址

				current_location_tail = current_location + numbytes - sizeof(struct mem_control_block);
				current_location_mcb_tail = (struct mem_control_block *)current_location_tail;
				current_location_mcb_tail->is_used = 1;  // 设为不可用
				current_location_mcb_tail->size = numbytes;
				

				break;
			}
		}
		// 否则，当前内存块不可用或过小，移动到下一个内存块
		current_location = current_location + current_location_mcb->size;
	}
	
	// 循环结束，没有找到合适的位置，需要向操作系统申请更多内存，但是在本系统里不会涉及拓展的问题
	if (!memory_location) {
		printf("RVOS need create a new page!!\n");
		
		// 扩展堆page
		void* page_new = page_alloc(1);
		_allocd_page_end = (void *)page_new + PAGE_SIZE;
		
		// 新的内存的起始位置就是 last_valid_address 的旧值
		memory_location = (void *)page_new;
		// 将 last_valid_address 后移 numbytes，移动到整个内存的最右边界
		//last_valid_address = last_valid_address + numbytes;
		last_valid_address = (void *)_allocd_page_end;
		// 初始化内存控制块 mem_control_block
		current_location_mcb = memory_location;
		current_location_mcb->is_used = 1;
		current_location_mcb->size = numbytes;
	}
	// 最终，memory_location 保存了大小为 numbyte的内存空间，
	// 并且在空间的开始处包含了一个内存控制块，记录了元信息
	// 内存控制块对于用户而言应该是透明的，因此返回指针前，跳过内存分配块
	memory_location = memory_location + sizeof(struct mem_control_block);
	// 返回内存块的指针
	return memory_location;
}

uint8_t pre_is_used(void *ptr){
	struct mem_control_block *pre_mem_tail = ptr - 2*sizeof(struct mem_control_block);
	return pre_mem_tail->is_used;
}

uint8_t pre_mem_size(void *ptr){
	struct mem_control_block *pre_mem_tail = ptr - 2*sizeof(struct mem_control_block);
	return pre_mem_tail->size;
}

uint8_t next_is_used(void *ptr){
	struct mem_control_block *mem_head = ptr - sizeof(struct mem_control_block);
	struct mem_control_block *next_mem_head = ptr - sizeof(struct mem_control_block) + mem_head->size;
	return next_mem_head->is_used;
}

uint8_t next_mem_size(void *ptr){
	struct mem_control_block *mem_head = ptr - sizeof(struct mem_control_block);
	struct mem_control_block *next_mem_head = ptr - sizeof(struct mem_control_block) + mem_head->size;
	return next_mem_head->size;
}

void my_free(void *ptr) {  // ptr 是要回收的空间
	struct mem_control_block *free;
	free = ptr - sizeof(struct mem_control_block); // 找到该内存块的控制信息的地址
	
	
	printf("ptr: 0x%x, pre is used: %d\n", ptr, pre_is_used(ptr));
	printf("ptr: 0x%x, pre mem size: %d\n", ptr, pre_mem_size(ptr));
	printf("ptr: 0x%x, next is used: %d\n", ptr, next_is_used(ptr));
	printf("ptr: 0x%x, next mem size: %d\n", ptr, next_mem_size(ptr));	
	


	uint8_t preIsUsed = pre_is_used(ptr);
	uint8_t nextIsUsed = next_is_used(ptr);
	uint32_t preMemSize = pre_mem_size(ptr);
	uint32_t nextMemSize = next_mem_size(ptr);
	if(preIsUsed && !nextIsUsed){
		// 后面的内存块空闲
		if(nextMemSize == 0){
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;
			free->is_used = 0;  // 该空间置为可用
			free_tail->is_used = 0;  // 该空间置为可用
		}else{
			uint32_t newSize = free->size + nextMemSize;
			struct mem_control_block *next_mem_tail = ptr - 2*sizeof(struct mem_control_block) + free->size + nextMemSize;
			struct mem_control_block *next_mem_head = ptr - sizeof(struct mem_control_block) + free->size;
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;

			next_mem_head->is_used = 0;
			next_mem_head->size = 0;
			free_tail->is_used = 0;
			free_tail->size = 0;

			next_mem_tail->is_used = 0;
			next_mem_tail->size = newSize;
			free->is_used = 0;  // 该空间置为可用
			free->size = newSize;			
		}
		

	}else if(!preIsUsed && nextIsUsed){
		// 前面的内存块空闲
		if(preMemSize == 0){
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;
			free->is_used = 0;
			free_tail->is_used = 0;  // 该空间置为可用
		}else{
			uint32_t newSize = free->size + preMemSize;
			struct mem_control_block *pre_mem_head = ptr - sizeof(struct mem_control_block) - preMemSize;
			struct mem_control_block *pre_mem_tail = ptr - 2*sizeof(struct mem_control_block);
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;

			free->is_used = 0;
			free->size = 0;	
			pre_mem_tail->is_used = 0;
			pre_mem_tail->size = 0;

			pre_mem_head->is_used = 0;
			pre_mem_head->size = newSize;
			free_tail->is_used = 0;  // 该空间置为可用
			free_tail->size = newSize;
		}
		
	}else if(!preIsUsed && !nextIsUsed){
		// 前后的内存块空闲
		if(nextMemSize == 0 && preMemSize == 0){
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;
			free->is_used = 0;  // 该空间置为可用
			free_tail->is_used = 0;  // 该空间置为可用
		}else{
			uint32_t newSize = free->size + preMemSize + nextMemSize;
			
			struct mem_control_block *pre_mem_head = ptr - sizeof(struct mem_control_block) - preMemSize;
			struct mem_control_block *pre_mem_tail = ptr - 2*sizeof(struct mem_control_block);
			struct mem_control_block *free_tail = ptr - 2*sizeof(struct mem_control_block) + free->size;
			struct mem_control_block *next_mem_head = ptr - sizeof(struct mem_control_block) + free->size;
			struct mem_control_block *next_mem_tail = ptr - sizeof(struct mem_control_block) + free->size + nextMemSize;

			pre_mem_tail->is_used = 0;
			pre_mem_tail->size = 0;
			free_tail->is_used = 0;
			free_tail->size = 0;
			next_mem_head->is_used = 0;
			next_mem_head->size = 0;
			
			pre_mem_head->is_used = 0;
			pre_mem_head->size = newSize;
			next_mem_tail->is_used = 0;
			next_mem_tail->size = newSize;
		}
		
	}

	return;
}

void showInfo(void *m_p, int blockSize){
	printf("m_p = 0x%x\n m_p is used: %d, m_p size is %d\n", m_p, ((struct mem_control_block *)(m_p-blockSize))->is_used, \
		((struct mem_control_block *)(m_p-blockSize))->size);
}

void page_test()
{
	printf("PAGE_LAST: 0x%x\n", PAGE_LAST);
	printf("managed_memory_start: 0x%x\n", managed_memory_start);
	printf("sizeof(struct mem_control_block): 0x%x\n", sizeof(struct mem_control_block));
	int blockSize = sizeof(struct mem_control_block);
	
	{
		printf("=======Test backward merge=======\n");
		void *m_p = my_malloc(32);
		showInfo(m_p, blockSize);
		void *m_p1 = my_malloc(32);
		showInfo(m_p1, blockSize);
		
		void *m_p2 = my_malloc(32);
		showInfo(m_p2, blockSize);
		
		printf("m_p = 0x%x, next_mem_size is %d\n", m_p2, next_mem_size(m_p2));
		my_free(m_p2);
		printf("m_p = 0x%x, next_mem_size is %d\n", m_p2, next_mem_size(m_p2));
		my_free(m_p1);
		//void *m_p3 = my_malloc(32);
		//showInfo(m_p3, blockSize);
		//my_free(m_p3);
		my_free(m_p);
	}

	void *m_p3 = 0x8000d008;
	showInfo(m_p3, blockSize);


	{
		// 测试向前合并
		printf("=======Test forward merge=======\n");
		void *m_p = my_malloc(32);
		showInfo(m_p, blockSize);
		void *m_p1 = my_malloc(32);
		showInfo(m_p1, blockSize);
		
		void *m_p2 = my_malloc(32);
		showInfo(m_p2, blockSize);
		my_free(m_p);
		my_free(m_p1);
		my_free(m_p2);
		//void *m_p3 = my_malloc(32);
		//showInfo(m_p3, blockSize);
	}

	{
		// 测试前后合并
		printf("=======Test back/forward merge=======\n");
		void *m_p = my_malloc(32);
		showInfo(m_p, blockSize);
		void *m_p1 = my_malloc(32);
		showInfo(m_p1, blockSize);
		
		void *m_p2 = my_malloc(2);
		showInfo(m_p2, blockSize);
		my_free(m_p);
		my_free(m_p2);
		my_free(m_p1);
		//void *m_p3 = my_malloc(32);
		//showInfo(m_p3, blockSize);
	}	
}

