#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <alloca.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

extern void Qsort(int l, int r, int d);

int bss1, bss2, bss3, bss[1000];
int data1 = 66, data2 = 77, data3 = 88;
int a[15] = {124, 51, 613, 513, 531, 
    63647, 315, 7426246, 3613, 13516, 
    1305, 1075, 130571, 10597, 10395};

int main() {
    char *p, *b, *bs;
// Text Segment, including pointers to two functions
    printf("Text Locations:\n");
    printf("\tAddress of main: %p\n", main);
    printf("\tAddress of qsort: %p\n", Qsort);

// Stack Segment
    printf("Stack Locations:\n");

    // Quick sort, including recursive call
    Qsort(0, 14, 1);

    // Use `alloca` to allocate memory in stack
    p = (char *) alloca(256);
    if (p) {
        printf("\tStart of alloca()'ed array: %p\n", p);
        printf("\tEnd of alloca()'ed array: %p\n", p+255);
        // Try to access address out of allocated space
        printf("\tOut of alloca()'ed array: %p\n", p+1023);
    }
// Data Segment
    printf("Data Locations:\n");
    printf("\tAddress of data1: %p\n", &data1);
    printf("\tAddress of data3: %p\n", &data3);
    printf("\tAddress of a[0]: %p\n", a);
    printf("\tAddress of a[10]: %p\n", a + 10);

// BSS Segment
    printf("BSS Locations:\n");
    printf("\tAddress of bss1: %p\n", &bss1);
    printf("\tAddress of bss3: %p\n", &bss3);
    printf("\tAddress of bss[0]: %p\n", bss);
    printf("\tAddress of bss[256]: %p\n", bss + 256);

// Heap Segment
    printf("Heap Locations:\n");
    b = sbrk((ptrdiff_t) 256);
    printf("\tInitial end of heap: %p\n", b);
    b = sbrk((ptrdiff_t) -128);
    printf("\tSecond end of heap: %p\n", b);
    b = sbrk((ptrdiff_t) -127);
    printf("\tThird end of heap: %p\n", b);
    b = sbrk((ptrdiff_t) 0);
    printf("\tFinal end of heap: %p\n", b);

// Mmap Space, should be in between stack and heap
    printf("Middle Location:\n");
    int bs_fd = open("addresses.txt", O_RDONLY);
    // mmap(start, length, prot, flags, fd, offset)
    bs = mmap(0, 256, PROT_READ, MAP_PRIVATE, bs_fd, 0);
    printf("\tMmap start address: %p\n", bs);
    printf("\tMmap end address: %p\n", bs + 256);
    sleep(3600);
    return 0;
}

void Qsort(int l, int r, int d) {
    auto int stack_var;
    if (d < 5) printf("\tDepth: %d, address: %p\n", d, &stack_var);
    int i = l, j = r, m = l + (r-l)/2;
    do {
        while(a[i] < a[m]) ++i;
        while(a[j] > a[m]) --j;
        if (i <= j) {
            int tmp = a[i];  a[i] = a[j];  a[j] = tmp;  ++i; --j;
        }
    } while (i <= j);
    if (l < j) Qsort(l, j, d+1);
    if (i < r) Qsort(i, r, d+1);
}