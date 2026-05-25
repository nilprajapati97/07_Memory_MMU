/* Approach 2: Arithmetic Swap */
#include <stdio.h>

void arithmetic_swap(int *a, int *b) {
    if (a != b) {
        *a = *a + *b;
        *b = *a - *b;
        *a = *a - *b;
    }
}

int main() {
    int x = 10, y = 20;
    printf("Before: x=%d, y=%d\n", x, y);
    arithmetic_swap(&x, &y);
    printf("After: x=%d, y=%d\n", x, y);
    return 0;
}
