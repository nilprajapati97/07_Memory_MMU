/* Approach 2: Union (Byte Access) */
#include <stdio.h>

unsigned int swap_endian(unsigned int x) {
    union {
        unsigned int i;
        unsigned char c[4];
    } src, dst;
    
    src.i = x;
    dst.c[0] = src.c[3];
    dst.c[1] = src.c[2];
    dst.c[2] = src.c[1];
    dst.c[3] = src.c[0];
    
    return dst.i;
}

int main() {
    unsigned int num = 0x12345678;
    printf("Original: 0x%08X\n", num);
    printf("Swapped:  0x%08X\n", swap_endian(num));
    return 0;
}
