/* Approach 3: Multiplication/Division Swap */
#include <stdio.h>

void multiply_swap(int *a, int *b) {
    if (a != b && *a != 0 && *b != 0) {
        *a = *a * *b;
        *b = *a / *b;
        *a = *a / *b;
    }
}

int main() {
    int x = 10, y = 20;
    printf("Before: x=%d, y=%d\n", x, y);
    multiply_swap(&x, &y);
    printf("After: x=%d, y=%d\n", x, y);
    return 0;
}
