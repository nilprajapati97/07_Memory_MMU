/* Approach 1: Standard Rotation */
#include <stdio.h>

#define BITS 32

unsigned int rotate_left(unsigned int n, unsigned int d) {
    return (n << d) | (n >> (BITS - d));
}

unsigned int rotate_right(unsigned int n, unsigned int d) {
    return (n >> d) | (n << (BITS - d));
}

int main() {
    unsigned int num = 16;
    int d = 2;
    
    printf("Original: %u (0x%X)\n", num, num);
    printf("Left rotate by %d: %u (0x%X)\n", d, rotate_left(num, d), rotate_left(num, d));
    printf("Right rotate by %d: %u (0x%X)\n", d, rotate_right(num, d), rotate_right(num, d));
    
    return 0;
}
