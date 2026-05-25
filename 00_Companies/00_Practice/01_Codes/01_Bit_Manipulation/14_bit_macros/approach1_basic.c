/* Approach 1: Basic Macros */
#include <stdio.h>

#define SETBIT(num, n)    ((num) |= (1U << (n)))
#define CLEARBIT(num, n)  ((num) &= ~(1U << (n)))
#define TOGGLEBIT(num, n) ((num) ^= (1U << (n)))
#define CHECKBIT(num, n)  (((num) >> (n)) & 1U)

int main() {
    unsigned int num = 20; // 10100
    
    printf("Original: %u (0x%X)\n", num, num);
    
    SETBIT(num, 0);
    printf("After SETBIT(0): %u (0x%X)\n", num, num);
    
    CLEARBIT(num, 2);
    printf("After CLEARBIT(2): %u (0x%X)\n", num, num);
    
    TOGGLEBIT(num, 3);
    printf("After TOGGLEBIT(3): %u (0x%X)\n", num, num);
    
    printf("CHECKBIT(4): %u\n", CHECKBIT(num, 4));
    
    return 0;
}
