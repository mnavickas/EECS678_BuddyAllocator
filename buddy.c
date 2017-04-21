/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 1

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "buddy.h"
#include "list.h"
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

/**************************************************************************
 * Public Definitions
 **************************************************************************/

#define CLEAR(x) x = -1
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int index;
	char *address;
	int order;
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
const char const g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[ (1<<MAX_ORDER) /PAGE_SIZE];

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;
	for (i = 0; i < n_pages; i++)
	{
		INIT_LIST_HEAD(&g_pages[i].list);
		g_pages[i].index = i;
		g_pages[i].address = PAGE_TO_ADDR(i);
		CLEAR(g_pages[i].order);
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++)
	{
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
}

/*
* Split a memory block into smaller chunks
*
* @param i list index
* @param order requested order
* @return memory block address
*/

void *buddy_split( int i, const int order)
{
	#if USE_DEBUG == 1
		PDEBUG("Split called on order %d, size %dK\n", order, (int)pow(2,order)/1024);
	#endif
	page_t *left_temp_ptr;
	// it fits, no need to break it down
	if( order == i )
	{
		left_temp_ptr = list_entry( free_area[i].next, page_t, list);
		list_del( &left_temp_ptr->list );
	}
	/*
	4. Remove a page from the order-i list and repeatedly break the page and
	populate the respective free-lists until the page of required-order is
	obtained. Return that page to caller (It would be good to encase this
	functionality in a separate function e.g. split)
	*/
	// need to break it down.
	else
	{
		#if USE_DEBUG == 1
			PDEBUG("Recursing.\n");
		#endif
		void *block_address = buddy_alloc( 1 << (order + 1) );
		left_temp_ptr = &g_pages[ADDR_TO_PAGE( block_address )];

		list_add(
				&g_pages[ left_temp_ptr->index + ( 1 << order ) / PAGE_SIZE ].list,
				&free_area[order]
				);
	}
	left_temp_ptr->order = order;
	return PAGE_TO_ADDR( left_temp_ptr->index );
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{

	#if USE_DEBUG == 1
		buddy_dump();
		PDEBUG("Allocating Size of %dK\n", size/1024);
	#endif
	if( size <= 0 )
	{
		return NULL;
	}
	/*
	1. Ascertain the free-block order which can satisfy the requested size.
	 The block order for size x is ceil ( log2 (x))
	Example:
	60k -> block-order = ceil ( log2 (60k)) =
	ceil ( log2 (k x 2^5 x 2^10)) = order-16
	*/
	const int REQ_ORDER = MAX( ceil( log2( size ) ), MIN_ORDER );


	for(int i = REQ_ORDER; i <= MAX_ORDER; i++ )
	{
		/*
		2. Iterate over the free-lists; starting from the order calculated in the
		above step. If the free-list at the required order is not-empty, just remove
		the first page from that list and return it to caller to satisfy the request
		*/
		if( !list_empty(&free_area[i]) )
		{
			return buddy_split(i, REQ_ORDER);
		}
		else
		{
		/*
		3. If the free-list at the required order is empty, find the first
		non-empty free-list with order > required-order. Lets say that such a
		list exists at order-i
		*/
		}
	}
	/*
	5. If a non-empty free-list is not found, this is an error
	*/
	return NULL;
}

/**
 * Find allocated memory block to free. buddy_free recursive helper.
 *
 *
 * @param address memory block address to be freed
 * @param order order of the block
 */
page_t *find_buddy(char const * const address, const int order)
{
	struct list_head *current;
	list_for_each( current, &free_area[order] )
	{
		page_t *current_page = list_entry(current, page_t, list);

		if(NULL == current_page ||  BUDDY_ADDR(address,order) == current_page->address )
		{
			return current_page;
		}
	}
	return NULL;
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{

	char* address = (char*)addr;
	/*
	1. Calculate the address of the buddy
	*/
	int index = ADDR_TO_PAGE ( address );
	int order = g_pages[index].order;

	/*
	2. If the buddy is free, merge the two blocks i.e. remove the buddy from its
	 free-list, update the order of the page-at-hand and add the page to the
	 relevant free-list
	*/

	/*
	3. Do step-2 repeatedly until no merging is possible
		a. The buddy is not free
		b. The max order is reached
	*/
	do
	{
		page_t *const current_page = find_buddy(address,order);

		if( NULL == current_page || BUDDY_ADDR(address, order) != current_page->address )
		{
			CLEAR(g_pages[index].order);
			list_add( &g_pages[index].list, &free_area[order] );
			return;
		}

		if( address > current_page->address )
		{
			address = current_page->address;
			index = ADDR_TO_PAGE( address );
		}

		list_del( &current_page->list );

	} while( order++ < MAX_ORDER );

}

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
