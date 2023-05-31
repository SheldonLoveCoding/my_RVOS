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
static uint32_t _alloc_end = 0;
static uint32_t _num_pages = 0;
static uint32_t _mlloc_start = 0;
static uint32_t _mlloc_end = 0;

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
// 将该page block  clear是什么意思？
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
// 判断该page block是否是连续开辟的最后一块block
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
	return (address + order) & (~order); // 把低12为置0，是为了方便内存整齐？
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

void *my_malloc(size_t size)
{
	//首先判断有没有分配page
	void *p = page_alloc(1); //分配一个试一下
	if(p != (void *)_alloc_start){
		//说明之前分配过，从之前分配过的找，把这个free掉
		//printf("之前分配过\n");
		page_free(p);
	}

	//在已分配的page里查找
	uint32_t res = 0;
	struct Page *page_i = (struct Page *)HEAP_START;
	for (int i = 0; i <= (_num_pages); i++){
		if (_is_not_free(page_i) && _is_not_full(page_i)){
			if((_mlloc_end >= _alloc_start + i * PAGE_SIZE) && (_mlloc_end < _alloc_start + (i+1) * PAGE_SIZE)){
				if(_mlloc_end + size < _alloc_start + (i+1) * PAGE_SIZE){
					res = _mlloc_end;
					_mlloc_end = _mlloc_end + size;
					return (void *)res;
				}else{
					printf("this page is not enough!\n ");
					_set_flag(page_i, PAGE_FULL);
					return NULL;
				}
			}else{
				printf("_mlloc_end is not in this page!\n ");
				return NULL;
			}
		}
		page_i++;
	}
	return NULL;
}

// free掉后续的字节
void my_free(void *ptr)
{
	if(ptr == NULL){
		return;
	}

	_mlloc_end = (uint32_t) ptr;
	return;
}

void page_test()
{
	printf("PAGE_LAST: 0x%x\n", PAGE_LAST);

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
}

