/* Approach 3: GCC Builtin (Best) */
#include <stdio.h>

unsigned int swap_endian(unsigned int x) {
    return __builtin_bswap32(x);
}

int main() {
    unsigned int num = 0x12345678;
    printf("Original: 0x%08X\n", num);
    printf("Swapped:  0x%08X\n", swap_endian(num));
    
    // Also available:
    // __builtin_bswap16() for 16-bit
    // __builtin_bswap64() for 64-bit
    
    return 0;
}
