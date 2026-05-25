/* Approach 1: Bit Shifting (Manual) */
#include <stdio.h>

unsigned int swap_endian(unsigned int x) {
    return ((x >> 24) & 0xFF) |
           ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) |
           ((x << 24) & 0xFF000000);
}

int main() {
    unsigned int num = 0x12345678;
    printf("Original: 0x%08X\n", num);
    printf("Swapped:  0x%08X\n", swap_endian(num));
    return 0;
}
