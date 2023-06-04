#include "os.h"

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
static uint32_t _allocd_page_end = 0;
static uint32_t _alloc_end = 0;
static uint32_t _num_pages = 0;
static uint32_t _mlloc_start = 0;
static uint32_t _mlloc_end = 0;
static uint32_t _mlloc_initialized = 0;     // 初始化malloc标志
void *managed_memory_start;  // 指向堆底（内存块起始位置）
void *last_valid_address;    // 指向堆顶

#define PAGE_SIZE 4096
#define PAGE_ORDER 12 // page的大小 量级 2^12 = 4K

// 相当于掩码
#define PAGE_TAKEN (uint8_t)(1 << 0)
#define PAGE_LAST  (uint8_t)(1 << 1)
#define PAGE_FULL  (uint8_t)(1 << 2) //判断该page是否还存在空余内存可以提供malloc

/*
 * Page Descriptor 
 * flags:
 * - bit 0: flag if this page is taken(allocated) 1-taken 0-not taken
 * - bit 1: flag if this page is the last page of the memory block allocated 1-last 0-not last
 */
struct Page {
	uint8_t flags;
};
/*
 * 内存控制块，用来描述malloc的开辟的nbytes内存块的信息
 */
struct mem_control_block {
  uint8_t is_used; // 是否可用（如果还没被分配出去，就是 0）
  uint8_t size;         // 实际空间的大小
};

// 将该page block  clear是清楚标志位
static inline void _clear(struct Page *page)
{
	page->flags = 0;
}

// 判断该page block是否是满的
static inline int _is_not_full(struct Page *page)
{
	if (page->flags & PAGE_FULL) {
		return 0;
	} else {
		return 1;
	}
}

// 判断该page block是否是可以用的
static inline int _is_free(struct Page *page)
{
	if (page->flags & PAGE_TAKEN) {
		return 0;
	} else {
		return 1;
	}
}
static inline int _is_not_free(struct Page *page)
{
	if (page->flags & PAGE_TAKEN) {
		return 1;
	} else {
		return 0;
	}
}

// 将flag的对应位设置为 1
static inline void _set_flag(struct Page *page, uint8_t flags)
{
	page->flags |= flags;
}
// 判断该page 是否是连续开辟的最后一块page
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
	uint32_t order = (1 << PAGE_ORDER) - 1; // 4K-1  0000 1111 1111 1111 
	printf("order = 0x%x\n", order);
	return (address + order) & (~order); // 把低12为置0，是为了方便内存整齐
}

void page_init()
{
	/* 
	 * We reserved 8 Page (8 x 4096) to hold the Page structures.
	 * It should be enough to manage at most 128 MB (8 x 4096 x 4096) 
	 */
	// 计算page的个数，前8个page_size的大小用来存放page描述符
	_num_pages = (HEAP_SIZE / PAGE_SIZE) - 8; 
	printf("HEAP_START = %x, HEAP_SIZE = %x, num of pages = %d\n", HEAP_START, HEAP_SIZE, _num_pages);

	// 清空page空间
	struct Page *page = (struct Page *)HEAP_START;
	for (int i = 0; i < _num_pages; i++) {
		_clear(page);
		page++;	
	}

	_alloc_start = _align_page(HEAP_START + 8 * PAGE_SIZE);
	_mlloc_start = _alloc_start;
	_mlloc_end = _alloc_start;
	printf("HEAP_START + 8 * PAGE_SIZE = %x, _alloc_start = %x\n", HEAP_START + 8 * PAGE_SIZE, _alloc_start);
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
  void *current_location;  // 当前访问的内存位置
  struct mem_control_block *current_location_mcb;  // 只是作了一个强制类型转换
  
  void *memory_location;  // 这是要返回的内存位置。初始时设为
                          // 0，表示没有找到合适的位置
  if (!_mlloc_initialized) {
    malloc_init();
  }
  // 要查找的内存必须包含内存控制块，所以需要调整 numbytes 的大小
  numbytes = numbytes + sizeof(struct mem_control_block);
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
		if((current_location_mcb->size == 0) || 
				(current_location_mcb->size != 0 && current_location_mcb->size >= numbytes))
				{
					// 找到一个可用、大小适合的内存块
					current_location_mcb->is_used = 1;  // 设为不可用
					current_location_mcb->size = numbytes;
					memory_location = current_location;      // 设置内存地址
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

void my_free(void *ptr) {  // ptr 是要回收的空间
  struct mem_control_block *free;
  free = ptr - sizeof(struct mem_control_block); // 找到该内存块的控制信息的地址
  free->is_used = 0;  // 该空间置为可用
  return;
}

void page_test()
{
	printf("PAGE_LAST: 0x%x\n", PAGE_LAST);
	/*
	void *p = page_alloc(2);
	printf("p = 0x%x\n", p);
	
	void *m_p = my_malloc(32);
	printf("m_p = 0x%x\n", m_p);
	void *m_p1 = my_malloc(64);
	printf("m_p1 = 0x%x\n", m_p1);
	//my_free(m_p1);
	void *m_p2 = my_malloc(2);
	printf("m_p2 = 0x%x\n", m_p2);


	void *p2 = page_alloc(7);
	printf("p2 = 0x%x\n", p2);
	page_free(p2);

	void *p3 = page_alloc(4);
	printf("p3 = 0x%x\n", p3);
	printf("p3+16 = 0x%x\n", p3+16);
	*/
	void *p = my_malloc(128);
	printf("p = 0x%x\n", p); // 0x8000d002
	
	void *p1 = my_malloc(2); 
	printf("p1 = 0x%x\n", p1); // 0x8000d084
	my_free(p1);

	void *p2 = my_malloc(128);
	printf("p2 = 0x%x\n", p2); // 0x8000d088
	void *p3 = my_malloc(1);
	printf("p3 = 0x%x\n", p3); // 0x8000d084
		
}

