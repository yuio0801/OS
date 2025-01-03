#include <common.h>

#define KB 1024
#define MB (1024*1024)
#define PAGE_SIZE (4*KB)

struct kmem_cache_node;

struct Page{
    int flag;
    uintptr_t pfn;
    uintptr_t order;
    int cpu;
    unsigned int freelist[17];
    uintptr_t active;
    struct kmem_cache_node * belong;

    struct Page *nxt, *pre;
};

struct Free_area {
	struct Page	*free_list;
	uintptr_t	nr_free;
};
//A simple slab
//jump kmem_cache, build kmem_cache_node directly

uintptr_t myStart,myEnd;

void *my_malloc(size_t size)
{
    void *ret = NULL;
    size = size % 8 == 0 ? size : ((size >> 3) + 1) << 3;
    if(myStart + size <= myEnd)
    {
        ret = (void *)myStart;
        myStart += size;
        myStart = myStart % 8 == 0 ? myStart : ((myStart >> 3) + 1) << 3;
        // for (uint64_t *p = (uint64_t *)ret; p != (uint64_t *)myStart; p++) 
        // {
        //     *p = 0;
        // }
    }
    return ret;
}
#define MAX_ORDER 13 // 16MB

struct Buddy{
    mutexlock_t buddy_lock;
    struct Free_area free_area[MAX_ORDER];
};

uintptr_t Start, End;
uintptr_t max_order, pmsize;

struct Page *page_belong;
struct Buddy *buddy;
uintptr_t calc(uintptr_t size)
{
  //  assert(size != 0);
    size_t ret = 1;
    uintptr_t fac = 0;
    while(ret < size)
    {
        ret <<= 1;
        fac++;
    }
    //if(fac < 3) fac = 3;
    return fac;
}

inline uintptr_t page_order(struct Page *page)
{
    return page->order;
}

inline uintptr_t find_buddy_pfn(uintptr_t page_pfn, uintptr_t order)
{
	return page_pfn ^ (1 << order);
}
 inline uintptr_t page_is_buddy(struct Page *page, struct Page *pagebuddy,
							uintptr_t order)
{
	if (page_order(pagebuddy) == order) {
		return 1;
	}
	return 0;
}
inline void delete(struct Page *page, uintptr_t order)
{
    struct Free_area *area = &buddy->free_area[order];
    if(page->nxt)
    {
        page->nxt->pre = page->pre;
    }
    if(page->pre)
    {
        page->pre->nxt = page->nxt;
    }
    if(area->free_list == page)
    {
        area->free_list = page->nxt;
    }
    area->nr_free--;
    page->pre=page->nxt = NULL;

}

inline struct Page *get_page_from_free_area(struct Free_area *area)
{
    struct Page *ret = NULL;
    if(area->nr_free > 0)
    {
        ret = area->free_list;
    }
    return ret;
}

inline void insert(struct Page *page, uintptr_t order,uintptr_t ppfn)
{
    struct Free_area *area = &buddy->free_area[order];
    page->flag = 0;
    page->order = order;
    page->pfn = ppfn;

    page->nxt = area->free_list;
    if(area->free_list)
    {
        area->free_list->pre = page;
    }
    area->free_list = page;
    area->nr_free++;
    
}

inline void expand(struct Page *page, uintptr_t order, uintptr_t current_order, int cpu)
{
    while(current_order > order)
    {
        uintptr_t pfn2 = page->pfn + (1 << (current_order - 1));
        current_order--;
        insert(&page_belong[pfn2], current_order, pfn2);
    }
    page->flag = 1;
    page->order = order;
    page->cpu = cpu;
}

inline struct Page *rmqueue_smallest(uintptr_t order, int cpu)
{
    mutex_lock(&buddy->buddy_lock);
    uintptr_t current_order = order;
   // assert(0<=order&&order<=12);
    struct Free_area *area;
    struct Page *page = NULL;
    for (current_order = order; current_order < MAX_ORDER; ++current_order) {
        area = &buddy->free_area[current_order];
        page = get_page_from_free_area(area);
        if (!page)
            continue;
        
        delete(page, current_order);
        expand(page, order, current_order, cpu);
        break;
    }
    mutex_unlock(&buddy->buddy_lock);
    if(!page) return NULL;
    return page;
}

inline uintptr_t pfn_valid_within(uintptr_t pfn)
{
    if(0 <= pfn && pfn < pmsize && page_belong[pfn].flag == 0)
    {
        return 1;
    }
    return 0;
}

inline void free_page(uintptr_t pfn, int cpu)
{
    mutex_lock(&buddy->buddy_lock);
    struct Page *page = &page_belong[pfn];
    uintptr_t order = page->order;
    //assert(0 <= order && order <= 12);
    
    uintptr_t combined_pfn, buddy_pfn;
    struct Page *buddy_pair;
    while (order < MAX_ORDER - 1) 
    {
        //break;
        //if(order <= 6) break;
        buddy_pfn = find_buddy_pfn(pfn, order);
        if (!pfn_valid_within(buddy_pfn))
        {
            break;
        }
            
        buddy_pair = &page_belong[buddy_pfn];
        if(!page_is_buddy(page, buddy_pair, order))
        {
            break;
        }
            
        delete(buddy_pair, order);
        buddy_pair->flag = -1;
        combined_pfn = buddy_pfn & pfn;
        page = &page_belong[combined_pfn];
        pfn = combined_pfn;
        order++;
    }
    insert(page, order, page->pfn);
    mutex_unlock(&buddy->buddy_lock);
}

void print_buddy()
{
    uintptr_t cpu = cpu_current();
    printf("cpu:%d\n",cpu);
    for(uintptr_t i = 0; i <= max_order; ++i)
    {
        struct Free_area *area = &buddy->free_area[i];
        struct Page *page = area->free_list;
        if(page)
        {
            printf("%d:", i);
            while(page)
            {
                printf("%d->", page->pfn);
                page = page->nxt;
            }
            printf("\n");
        }
    }
}
//---------------------------
struct kmem_cache_node;
struct kmem_node;

struct kmem_cache_node {
    mutexlock_t kmem_cache_lock;

    //struct Page *slabs_full_head;
    struct Page *slabs_partial_head;

    size_t object_size;
    uintptr_t slab_limit;
};

struct kmem_node{
    struct kmem_cache_node kmem_cache[13];
};

struct kmem_node *kmem;

static inline uintptr_t get_obj_pos(void *ptr)
{
  struct Page *page = &page_belong[((uintptr_t)ptr >> 12) - Start];
  uintptr_t pos = ((uintptr_t )ptr - ((page->pfn + Start) << 12)) >> page->belong->object_size;
  return pos;
}

inline uintptr_t lowbit(uintptr_t x)
{
    return x & (-x);
}

inline struct Page * create_free_slab(struct kmem_cache_node *kmem_tar, size_t object_size, int cpu)
{
    struct Page *page = rmqueue_smallest(0, cpu);
    if(page == NULL)
    {
        return NULL;
    }
    page->cpu = cpu;
    page->belong = kmem_tar;
    int obj_num = kmem_tar->slab_limit;
    int bitmap_num = (obj_num % 32 == 0) ? (obj_num / 32) : (obj_num / 32 + 1);
    page->flag = 2;
    for(int i = 0; i < bitmap_num; ++i)
    {
        page->freelist[i] = -1;
    }
    page->active = 0;
    return page;
}

//It's faster when I remove the switching of full slab and partial slab
//I don't know why...


/*
inline void move_slab_partial_to_full(struct Page *page, struct kmem_cache_node *kmem_tar)
{
    if(page->nxt)
    {
        page->nxt->pre = page->pre;
    }
    if(page->pre)
    {
        page->pre->nxt = page->nxt;  
    }
    if(page == kmem_tar->slabs_partial_head)
    {
        kmem_tar->slabs_partial_head = page->nxt;
    }
    page->nxt = kmem_tar->slabs_full_head;
    if(page->nxt)
    {
        page->nxt->pre = page;
    }
    kmem_tar->slabs_full_head = page;
    page->pre = NULL;
    return ;
}

inline void move_slab_full_to_partial(struct Page *page, struct kmem_cache_node *kmem_tar)
{
    if(page->nxt)
    {
        page->nxt->pre = page->pre;
    }
    if(page->pre)
    {
        page->pre->nxt = page->nxt;  
    }
    if(page == kmem_tar->slabs_full_head)
    {
        kmem_tar->slabs_full_head = page->nxt;
    }
    page->nxt = kmem_tar->slabs_partial_head;
    if(page->nxt)
    {
        page->nxt->pre = page;
    }
    kmem_tar->slabs_partial_head = page;
    page->pre = NULL;
    return ;
}
*/

inline void free_slab(struct Page *page, struct kmem_cache_node *kmem_tar)
{
    if(page->nxt)
    {
        page->nxt->pre = page->pre;
    }
    if(page->pre)
    {
        page->pre->nxt = page->nxt;   
    }
    if(page == kmem_tar->slabs_partial_head)
    {
        kmem_tar->slabs_partial_head = page->nxt;
    }
    //assert(page->active == 0);
    page->flag = 0;
    page->belong = NULL;
    page->cpu = -1;
    page->pre = page->nxt = NULL;
    assert(page->order == 0);
    //free(page->freelist);
    return ;
}
//mutexlock_t cpu_lock[8];
//mutexlock_t bkl = mutex_init("asd");

inline static void *kalloc_in_slab(struct Page * page)
{
  void *ret=NULL;
  struct kmem_cache_node *kmem_tar = page->belong;
  int obj_num = kmem_tar->slab_limit;
  int bitmap_num = (obj_num % 32 == 0) ? (obj_num / 32) : (obj_num / 32 + 1);
  for(int i = 0;i < bitmap_num; i++)
  {
    if(page->freelist[i] == 0 || page->freelist[i] % ((uintptr_t)1 << 32) == 0) continue;
    int pos = calc(lowbit(page->freelist[i]));
    page->freelist[i] -= lowbit(page->freelist[i]);
    page->active ++;
    /*
    if(page->active == kmem_tar->slab_limit)
    {
        move_slab_partial_to_full(page, kmem_tar);
    }
    */
    return (void*)(((i * 32 + pos) << kmem_tar->object_size) + ((page->pfn + Start) << 12));  
  }
  return ret;
}


inline static void *kalloc(size_t size) {
    // TODO
    // You can add more .c files to the repo.
    if(size == 0) return NULL;
    uintptr_t fac = calc(size);
    if(fac > 24) return NULL;
    uintptr_t cpu = cpu_current();
    if(fac > 11) 
    {
        struct Page *ret = rmqueue_smallest(fac - 12, cpu);
        if(!ret) return NULL;
        return (void *)((Start + ret->pfn) << 12);
    }
    
    if(fac < 3) fac = 3;
    struct kmem_cache_node *kmem_tar = &kmem[cpu].kmem_cache[fac];

    mutex_lock(&kmem_tar->kmem_cache_lock);
    void * ret = NULL;

    struct Page *page =  kmem_tar->slabs_partial_head;
    while(page && page->active == kmem_tar->slab_limit) 
    {
        page=page->nxt;
    }
    if(!page)
    {
        page = create_free_slab(kmem_tar, fac, cpu);
        if(!page)
        {
            mutex_unlock(&kmem_tar->kmem_cache_lock);
            return NULL;
        }
        page->nxt = NULL;
        kmem_tar->slabs_partial_head = page;
        page->pre = NULL;

    }
    ret = kalloc_in_slab(page);
    mutex_unlock(&kmem_tar->kmem_cache_lock);
    return ret;
}

inline static void kfree(void *ptr) {
    // TODO
    // You can add more .c files to the repo.
    //return ;
    if(ptr == NULL) return ;
    // if(!(Start <= ((uintptr_t)ptr >> 12) && ((uintptr_t)ptr >> 12) < End))
    //     return ;
    struct Page * page = &page_belong[((uintptr_t)ptr >> 12) - Start];

    uintptr_t cpu = page->cpu;

    //assert(cpu == cpu_current());
    if(page->flag == 0 || page->flag == -1) return ;
    if(page->flag == 1)
    {
        free_page(page->pfn, cpu);
        return ;
    }
    //return ;
    struct kmem_cache_node * kmem_tar = page->belong;
    
    mutex_lock(&kmem_tar->kmem_cache_lock);
    /*
    if(page->active == kmem_tar->slab_limit)
    {
        move_slab_full_to_partial(page, kmem_tar);    
    }
    */
    //mutex_unlock(&kmem_tar->kmem_cache_lock);
    
    uintptr_t pos = get_obj_pos(ptr);
    page->active --;
   // assert(!(page->freelist[pos / 32] & (1 << (pos % 32))));
    page->freelist[pos / 32] ^= (1 << (pos % 32));
    /*
    if(page->active == 0)
    {
        free_slab(page, kmem_tar);
        free_page(page->pfn, cpu);
    }
    */
    mutex_unlock(&kmem_tar->kmem_cache_lock);
}

// 框架代码中的 pmm_init (在 AbstractMachine 中运行)
static void pmm_init() {
    Start = (uintptr_t)heap.start;
    End = (uintptr_t)heap.end;

    myStart = (Start % 8 == 0? Start:(Start >>3) << 3);
    myEnd = (End >> 3) << 3;

    Start = ((Start >> 12) + (Start % (1 << 12) != 0));
    End = (End >> 12);
    //seperate 4MB
    max_order = 12;
    Start = ((Start >> max_order) + (Start % (1 << max_order) != 0)) << max_order;
    End = (End >> max_order) << max_order;
    pmsize = (End - Start);

    /*printf(
        "Got %d MiB heap: [%u, %u)\n",
       pmsize >> 8, (uintptr_t)Start , (uintptr_t)End 
    );*/

    uintptr_t num_cpu = cpu_count();
    page_belong = my_malloc(pmsize * sizeof(struct Page));
    buddy = my_malloc(sizeof(struct Buddy));
    kmem = my_malloc(num_cpu * sizeof(struct kmem_cache_node));
    
    Start = ((myStart >> 12) + (myStart % (1 << 12) != 0));
    Start = ((Start >> max_order) + (Start % (1 << max_order) != 0)) << max_order;
    pmsize = (End - Start);

    for(uintptr_t i = 0; i < pmsize; ++i)
    {
        page_belong[i].flag = -1;
        page_belong[i].pfn = i;
        page_belong[i].cpu = -1;
        page_belong[i].belong = NULL;
        page_belong[i].nxt = page_belong[i].pre = NULL;
    }

    mutex_init(&buddy->buddy_lock, "buddy_lock");
    for(int j = 0;j <= 12; ++j)
    {
        buddy->free_area[j].free_list = NULL;
        buddy->free_area[j].nr_free = 0;
    }
    
    for(uintptr_t i = 0; i + (1 << max_order) <= pmsize; i += (1 << max_order))
    {
        struct Page *page = &page_belong[i];
        insert(page, max_order, i);
    }
    for(uintptr_t i = 0; i < num_cpu; ++i)
    {
        for(uintptr_t j = 3; j <= 12; ++j)
        {
            struct kmem_cache_node *kmem_tar = &kmem[i].kmem_cache[j];
            mutex_init(&kmem_tar->kmem_cache_lock, "kmem lock");
            kmem_tar->slab_limit = PAGE_SIZE >> j;
            kmem_tar->object_size = j;
            for(int k = 0;k < 4; ++k)
            {
                struct Page *page = create_free_slab(kmem_tar, j, i);
                page->nxt = NULL;
                kmem_tar->slabs_partial_head = page;
                page->pre = NULL;
            }
            kmem_tar->slabs_partial_head = NULL;
            //kmem_tar->slabs_full_head = NULL;
        }
    }

    
    //print_buddy();
    
}

static void *kalloc_safe(size_t size){
    bool st = ienabled();
    iset(false);
    void *ret = kalloc(size);
    if(st) 
    {
        iset(true);
    }
    return ret;
}

static void kfree_safe(void *ptr){
    bool st = ienabled();
    iset(false);
    kfree(ptr);
    if(st) 
    {
        iset(true);
    }
    return ;
}

MODULE_DEF(pmm) = {
    .init  = pmm_init,
    .alloc = kalloc_safe,
    .free  = kfree_safe,
};
