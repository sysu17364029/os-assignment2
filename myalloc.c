#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

struct control_block {
    int free;  // Free flag
    int size;  // block size, INCLUDING this control block
    void *prev;  // Previous block
};

// Start and end of heap address
void *start, *end;
// Address of last control block
void *tail;
int init = 0;

void *st() {return start;}
void *en() {return end;}

void *myalloc(unsigned n_bytes) {
// Init if it hasn't
    if (init == 0) {start = end = sbrk(0);  printf("Init %p\n", start);  init = 1;}
    void *curr = start, *alloc_address = NULL;
    struct control_block *curr_block;
// Compute total size of the block
    unsigned tot_size = n_bytes + sizeof(struct control_block);
// Align to 16 bytes
    tot_size += 16 - (tot_size % 16);
// Find the first free block which is big enough
    while (curr != end) {
        curr_block = (struct control_block *)curr;
        if (curr_block -> free == 1 && curr_block -> size >= tot_size) {
            curr_block -> free = 0;
            alloc_address = curr;
            break;
        }
        curr = curr + curr_block -> size;
    }
// Allocate heap address if don't find
    if (alloc_address == NULL) {
        if (sbrk(tot_size) == (void*) - 1) return NULL;
        alloc_address = end;
        curr_block = (struct control_block *)alloc_address;
        curr_block -> free = 0;
        curr_block -> size = tot_size;
        curr_block -> prev = tail;
        tail = curr_block;
        end += tot_size;
    }
// Return address of first byte
    printf("Allocate: %p-%p, prev: %p\n", (struct control_block *)alloc_address, alloc_address + ((struct control_block *)alloc_address) -> size, ((struct control_block *)alloc_address) -> prev);
    alloc_address += sizeof(struct control_block);
    return alloc_address;
}

void myfree(void *p) {
    printf("Free starts.\n");
    if (p == NULL || p < start + sizeof(struct control_block) || p > end + sizeof(struct control_block)) return;
// Recover the address of control block
    struct control_block *cb = (struct control_block *)(p - sizeof(struct control_block));
// Free the block
    cb -> free = 1;
    printf("\tend: %p, cb + size: %p\n", end, (struct control_block *)((long long)cb + cb -> size));
// If top of the heap is free, shrink the space of the heap
    if (cb == tail) {
        while (cb != NULL && cb -> free == 1) {
            int s = cb -> size;
            printf("\tFree %p-%p\n", cb, (long long)cb + cb -> size);
            tail = cb -> prev;
            cb = cb -> prev;
            sbrk(-s);
            end = end - s;
            printf("\ttail: %p, end: %p", tail, end);
            if (cb != NULL) printf(", free: %d", cb -> free);
            printf("\n");
        }
    }
    printf("Free Ends.\n");
    return;
}

char buff[50];

// Convenient function for calling `myalloc` and print the stack info via bash command
void* malloc_and_print(unsigned s, const char *info) {
    printf("%s\n", info);
    void * p = myalloc(s);
    print_stack();
    return p;
}

void print_stack() {
    sprintf(buff, "cat /proc/%d/maps | grep heap", getpid());
    system(buff);
}


int main() {
    int *a = malloc_and_print(sizeof(int), "Allocate int a...");
    long *b = malloc_and_print(sizeof(long), "Allocate long b...");
    int *c = malloc_and_print(sizeof(int), "Allocate int c...");
    int *d = malloc_and_print(65536 * sizeof(int), "Allocate int[65536] d...");
    char *e = malloc_and_print(sizeof(char), "Allocate char e...");
    d[65500] = 10;
    printf("%d\n", d[65500]);
    myfree(b);
    myfree(c);
    myfree(e);  // Block of `e` will be returned
    void *f = malloc_and_print(10, "Allocate 10 f...");  // Will be allocated the block belonged to `b`
    myfree(d);  // Block of `d` and `c` will be returned
    myfree(f);  // Block of `f`(`b`) will be returned
    myfree(a);  // Block of `a` will be returned
    printf("%p %p\n", st(), en());  // Will be equal
    print_stack();
}