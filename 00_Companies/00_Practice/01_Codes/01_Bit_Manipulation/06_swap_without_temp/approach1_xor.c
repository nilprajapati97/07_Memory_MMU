/* Approach 1: XOR Swap (Best for Bit Manipulation) */
#include <stdio.h>

void xor_swap(int *a, int *b) {
    if (a != b) {
        *a ^= *b;
        *b ^= *a;
        *a ^= *b;
    }
}

int main() {
    int x = 10, y = 20;
    printf("Before: x=%d, y=%d\n", x, y);
    xor_swap(&x, &y);
    printf("After: x=%d, y=%d\n", x, y);
    return 0;
}
