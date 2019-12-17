#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

// Number of virtual pages
#define V_PAGES 256
// Mask used to take the virtual page segment from address
#define VPAGE_MASK V_PAGES - 1
// Page size
#define PAGE_SIZE 256
// Mask used to take the offset segment from address
#define OFFSET_MASK PAGE_SIZE - 1
// Backing-store size
#define BS_SIZE V_PAGES * PAGE_SIZE

// Number of physical pages
int P_PAGES = 256;
// Mask used to take the physical page segment from address
#define PPAGE_MASK (P_PAGES - 1)
// Main memory size
#define MM_SIZE (P_PAGES * PAGE_SIZE)
// TLB size
#define TLB_SIZE 16
// Offset bit used to take the offset segment from address
#define OFFSET 8

// Replacement Strategy 
//Valid Options: fifo, FIFO, lru, LRU
char* RS = "FIFO";

struct TLB {
    int phy;  // Physical page
    int log;  // Logical page
    char prev;  // Index of the previous TLB term in double linked-list
    char next;  // Index of the next TLB term in double linked-list
} tlb[TLB_SIZE];
int tlb_head, tlb_tail, tlb_size;


struct PAGE {
    int phy;  // Physical page
    short prev;  // Index of the previous PT term in double linked-list
    short next;  // Index of the next PT term in double linked-list
} pt[V_PAGES];
int pt_head, pt_tail, pt_size;

char *mm;  // Main memory
char *bs;  // Backing-store file
FILE *fp;  // Address file pointer


/***********************************************************
*    Here we implement the find and the replace method of  *
*  both TLB and page table.                                *
************************************************************/


/***********************************************************
*    In `tlb_find_fifo`, we just search for the required   *
*  logical page. If exists, return the corresponding phy-  *
*  sical page; otherwise return -1.                        *
************************************************************/
int tlb_find_fifo(int log) {
    int i;
    for (i = 0; i < tlb_size; ++i) {
        if (tlb[i].log == log) return tlb[i].phy;
    }
    return -1;
}

/***********************************************************
*    In tlb_replace_fifo, we also just replace the earli-  *
*  est TLB term, which is followed by `tlb_tail`. We also  *
*  use variable `tlb_size` to keep track of the TLB size.  *
*  If TLB is not null, the searching in `tlb_find_fifo`    *
*  will shrink a little.                                   *
************************************************************/
void tlb_replace_fifo(int log, int phy) {
    if (tlb_size < TLB_SIZE) ++tlb_size;  // Track the TLB size
    tlb[tlb_tail].log = log;
    tlb[tlb_tail].phy = phy;
    tlb_tail = (tlb_tail + 1) % TLB_SIZE;  // Circular growth
}

/***********************************************************
*    We use a double linked-list to maintain the access of *
*  TLB terms. The recently visited term will be closer to  *
*  the head of the list. Once we find a TLB term, we need  *
*  to move it to the head.                                 *
************************************************************/

/* The change of the linkage:
 *   1. (head = p -> next) : return
 *   2. (prev -> p -> next) : (prev -> next) && (p -> head) && (head = p)
 *   3. (prev -> p = tail) : (p -> head) && (head = p) && (tail = prev)
 */
int tlb_find_lru(int log) {
// Find the logical page through the linked-list
    int i = tlb_head;
    while (i != -1 && tlb[i].log != log) {
        i = tlb[i].next;
    }
    if (i == -1) return -1;  // If not find, return -1
    if (i == tlb_head) return tlb[i].phy;  // If it is already the head, return
    tlb[tlb[i].prev].next = tlb[i].next;  // Link the previous node to the next node
    if (i != tlb_tail)
        tlb[tlb[i].next].prev = tlb[i].prev;  // Link the next node to the previous node
    else
        tlb_tail = tlb[i].prev;  // Update `tlb_tail` if needed
    tlb[i].next = tlb_head;  // Link the current node to the head
    tlb[i].prev = -1;
    tlb[tlb_head].prev = i;  // Link the old head node to the current node
    tlb_head = i;  // Update `tlb_head`
    return tlb[i].phy;
}

/***********************************************************
*  When we need to replace, there are three situations:    *
*    1. When TLB is empty, initialize the linked-list      *
*    2. When TLB is neither empty nor full, link the new   *
*    node to the head                                      *
*    3. When TLB is full, replace the tail of the linked-  *
*    list, and link it to the head                         *
************************************************************/

/* The change of the linkage:
 *   1. (empty) : (head = p = tail)
 *   2. (not empty) && (not full) : (p -> head) && (head = p)
 *   3. (full) && (p -> tail) : (q replace tail) && (tail = p) && (q -> head) && (head = q)
 */

void tlb_replace_lru(int log, int phy) {
// If empty, initialize the linked-list
    if (tlb_size == 0) {
        tlb_head = tlb_tail = 0;  tlb_size = 1;
        tlb[0].log = log;  tlb[0].phy = phy;
        return;
    }
// If not full, link the new node to the head
    if (tlb_size < TLB_SIZE) {
        tlb[tlb_head].prev = tlb_size;
        tlb[tlb_size].next = tlb_head;
        tlb[tlb_size].log = log;
        tlb[tlb_size].phy = phy;
        tlb_head = tlb_size;
        ++tlb_size;
        return;
    }
// If full, replace tail and link it to the head
    int i = tlb_tail;
    tlb[tlb[i].prev].next = -1;  // Unlink tail
    tlb_tail = tlb[i].prev;  // Update `tlb_tail`
    tlb[i].next = tlb_head;  // Link new node to head
    tlb[i].prev = -1;
    tlb[tlb_head].prev = i;  // Link head to new node
    tlb_head = i;  // Update `tlb_head`
    tlb[i].log = log;  tlb[i].phy = phy;  // Update term
}

/* PS: Since the TLB is implemented by an array, we can just
 *   SWAP the current term and the head term to simulate the
 *   'recent used' feature, and thus can be free from double
 *   linked-list.
 * 
 * e.g. when (access p) && (prev -> p -> next) : (SWAP p and head)
 */



/***********************************************************
*    Since the index of a logical page is fixed in a page  *
*  table, we can no longer use the style in what TLB uses. *
*  Instead, we use a single linked-list to implement the   *
*  FIFO page table replacement. In `pt_find_fifo`, we can  *
*  directly return the needed page.                        *
************************************************************/

int pt_find_fifo(int log) {
    return pt[log].phy;
}

/***********************************************************
*  When we need to replace, there are three situations:    *
*    1. When physical page is empty, initialize the list   *
*    2. When physical page is neither empty nor full, link *
*    the new node to the tail                              *
*    3. When physical page is full, delete the node at the *
*    head, allocate the physical page to the new logical   *
*    page, and link the new node to the tail.              *
************************************************************/

void pt_replace_fifo(int log) {
// If empty, initialize the list
    if (pt_size == 0) {
        pt[log].phy = 0;
        pt_head = pt_tail = log;  pt_size = 1;
        return;
    }
// If not full, link the new node to the tail
    if (pt_size < P_PAGES) {
        pt[log].phy = pt_size;
        pt[pt_tail].next = log;
        pt_tail = log;
        ++pt_size;
        return;
    }
    // Delete the node at the head
    int i = pt_head, phy = pt[i].phy;
    pt_head = pt[i].next;
    pt[i].phy = pt[i].next = -1;
    // Allocate the physical page to the new node
    pt[log].phy = phy;
    // Append the node to the tail
    pt[pt_tail].next = log;
    pt_tail = log;
}


/***********************************************************
*    Similar to `tlb_find_lru`, we use double linked-list  *
*  to maintain the access time. There is one difference:   *
*  we no longer need to search for the logical page in the *
*  linked-list, instead we can directly use it as an index.*
************************************************************/

/* The change of the linkage:
 *   1. (head = p -> next) : return
 *   2. (prev -> p -> next) : (prev -> next) && (p -> head) && (head = p)
 *   3. (prev -> p = tail) : (p -> head) && (head = p) && (tail = prev)
 */

int pt_find_lru(int log) {
    if (pt[log].phy == -1) return -1;  // If not exist, return -1
    int i = log;
    if (i == pt_head) return pt[i].phy;  // If it is head, return
    pt[pt[i].prev].next = pt[i].next;  // Link prev to next
    if (i != pt_tail)
        pt[pt[i].next].prev = pt[i].prev;  // Link next to prev
    else
        pt_tail = pt[i].prev;  // Update `pt_tail` if needed
    pt[i].next = pt_head;  // Link current to head
    pt[i].prev = -1;
    pt[pt_head].prev = i;  // Link old head to current
    pt_head = i;  // Update `pt_head`
    return pt[i].phy;
}

/***********************************************************
*  When we need to replace, there are three situations:    *
*    1. When PT is empty, initialize the linked-list       *
*    2. When PT is neither empty nor full, link the new    *
*    node to the head                                      *
*    3. When PT is full, drop the tail of the linked-list, *
*    allocate the physical page to the new node, and link  *
*    it to the head                                        *
************************************************************/

/* The change of the linkage:
 *   1. (empty) : (head = p = tail)
 *   2. (not empty) && (not full) : (p -> head) && (head = p)
 *   3. (full) && (p -> tail) && (new q) : (q->phy = tail->phy) && (tail = p) && (q -> head) && (head = q)
 */

void pt_replace_lru(int log) {
// If empty, initialize the list
    if (pt_size == 0) {
        pt[log].phy = 0;
        pt_head = pt_tail = log;  pt_size = 1;
        return;
    }
// If not full, link the node to the head
    if (pt_size < P_PAGES) {
        pt[log].phy = pt_size;
        pt[pt_head].prev = log;
        pt[log].next = pt_head;
        pt_head = log;
        ++pt_size;
        return;
    }
    // Delete the tail node of the list
    int i = pt_tail, phy = pt[i].phy;
    pt[pt[i].prev].next = -1;
    pt_tail = pt[i].prev;
    pt[i].phy = pt[i].prev = pt[i].next = -1;
    // Allocate the physical page to the new page
    pt[log].phy = phy;
    // Append the node to the head of the list
    pt[log].next = pt_head;
    pt[log].prev = -1;
    pt[pt_head].prev = log;
    pt_head = log;
}

// Function pointers

int (*tlb_find)(int);
void (*tlb_replace)(int, int);
int (*pt_find)(int);
void (*pt_replace)(int);

void Init(int argc, const char *argv[]) {
    int bs_fd = open(argv[1], O_RDONLY);
    // mmap(start, length, prot, flags, fd, offset)
    bs = mmap(0, BS_SIZE, PROT_READ, MAP_PRIVATE, bs_fd, 0);

    fp = fopen(argv[2], "r");
// Initialize page table and TLB
    int i = 0;
    pt_head = -1;  pt_tail = -1;  pt_size = 0;
    for (; i < V_PAGES; ++i) {
        pt[i].phy = -1;  pt[i].prev = -1;  pt[i].next = -1;
    }

    tlb_head = -1;  tlb_tail = -1;  tlb_size = 0;
    for (i = 0; i < TLB_SIZE; ++i) {
        tlb[i].prev = -1;  tlb[i].next = -1;
    }

	int opt;
	char *string = "n:p:";
	while((opt = getopt(argc, argv, string))!= -1)
	{
		// printf("%c %s\n", opt, optarg);
		if (opt == 'n')
			P_PAGES = atoi(optarg);
		else if (opt == 'p')
			RS = optarg;
	}
	mm = malloc(MM_SIZE);

// Choose methods
	if (strcmp(RS, "lru") == 0 || strcmp(RS, "LRU") == 0)
	{
		tlb_find = tlb_find_lru;
		tlb_replace = tlb_replace_lru;
		pt_find = pt_find_lru;
		pt_replace = pt_replace_lru;
	}
	else if (strcmp(RS, "fifo") == 0 || strcmp(RS, "FIFO") == 0)
	{
		tlb_find = tlb_find_fifo;
		tlb_replace = tlb_replace_fifo;
		pt_find = pt_find_fifo;
		pt_replace = pt_replace_fifo;
	}
	else
	{
		printf("Invalid strategy!\n");
		exit(1);
	}
}

int main(int argc, const char *argv[]) {
    if (argc < 3) {
		printf("usage:./vm bs addresses.txt -p replacement_strategy -n n_physical_pages\n");
        return 1;
    }
	//printf("%s %s\n", argv[1], argv[2]);
    Init(argc,argv);

    int logical_address, tlb_hit = 0, page_fault = 0;
    while (fscanf(fp, "%d", &logical_address) != EOF) {
        int offset = logical_address & OFFSET_MASK;  // Take offset
        int logical_page = (logical_address >> OFFSET) & VPAGE_MASK;  // Take logical page
        int physical_page = tlb_find(logical_page);  // Find physical page in TLB
        // TLB hit
        if (physical_page != -1) {
            ++tlb_hit;
        }
        // TLB miss
        else {
            physical_page = pt_find(logical_page);  // Find physical page in page table
            // page fault
            if (physical_page == -1) {
                ++page_fault;
                pt_replace(logical_page);  // Update page table
                physical_page = pt[logical_page].phy;  // Get physical page
                // memcpy(dst, src, n)
                memcpy(mm + physical_page * PAGE_SIZE, bs + logical_page * PAGE_SIZE, PAGE_SIZE);  // Copy data to main memory
            }
            tlb_replace(logical_page, physical_page);  // Update TLB
        }
        int physical_address = (physical_page << OFFSET) | offset;  // Calc physical address
        char val = mm[physical_address];  // Get the value
        // printf("l: %d, p: %d\n", logical_address, physical_address);
        // printf("%d %d\n", tlb_hit, page_fault);
    }
    printf("Page Faults = %d\n", page_fault);
    printf("TLB Hits = %d\n", tlb_hit);
}