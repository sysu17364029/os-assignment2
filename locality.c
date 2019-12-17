#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <malloc.h>
#include <unistd.h>

int a[10][10000], c[10005], b[10000][10], ans[10][10];


int main() {
    freopen("addresses_locality.txt", "w", stdout);
    int cnt = 0;
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10000; ++k) {
                int *x = (int*)sbrk(sizeof(int)), *y = (int*)sbrk(sizeof(int)), *z = (int*)sbrk(sizeof(int));
                sbrk(256);
                *x = (long long)(&a[i][k]) % 65536;  *y = (long long)(&b[k][j]) % 65536;  *z = (long long)(&ans[i][j]) % 65536;
                cnt += 6;
                printf("%d\n%d\n%d\n", *x % 65536, *y % 65536, *z % 65536);
                printf("%lld\n%lld\n%lld\n", (long long)(x) % 65536, (long long)(y) % 65536, (long long)(z) % 65536);
                if (cnt >= 10000) return 0;
            }
        }
    }
}
