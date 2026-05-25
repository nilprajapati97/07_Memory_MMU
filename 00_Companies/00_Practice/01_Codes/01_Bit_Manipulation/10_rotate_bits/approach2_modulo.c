/* Approach 2: With Modulo (Safe) */
#include <stdio.h>

#define BITS 32

unsigned int rotate_left(unsigned int n, unsigned int d) {
    d %= BITS;  // Handle d >= BITS
    return (n << d) | (n >> (BITS - d));
}

unsigned int rotate_right(unsigned int n, unsigned int d) {
    d %= BITS;
    return (n >> d) | (n << (BITS - d));
}

int main() {
    unsigned int num = 16;
    int d = 34;  // > 32 bits
    
    printf("Original: %u\n", num);
    printf("Left rotate by %d: %u\n", d, rotate_left(num, d));
    printf("Right rotate by %d: %u\n", d, rotate_right(num, d));
    
    return 0;
}
