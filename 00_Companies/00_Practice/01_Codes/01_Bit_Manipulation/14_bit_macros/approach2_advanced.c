/* Approach 2: Advanced Macros */
#include <stdio.h>

// Basic operations
#define BIT(n)              (1U << (n))
#define SETBIT(num, n)      ((num) |= BIT(n))
#define CLEARBIT(num, n)    ((num) &= ~BIT(n))
#define TOGGLEBIT(num, n)   ((num) ^= BIT(n))
#define CHECKBIT(num, n)    (((num) & BIT(n)) != 0)

// Multi-bit operations
#define SETBITS(num, mask)    ((num) |= (mask))
#define CLEARBITS(num, mask)  ((num) &= ~(mask))
#define TOGGLEBITS(num, mask) ((num) ^= (mask))

// Create mask
#define MASK(n)             (BIT(n) - 1)  // n bits set
#define BITMASK(h, l)       (MASK((h)-(l)+1) << (l))  // bits h to l

// Extract field
#define GETFIELD(num, h, l) (((num) >> (l)) & MASK((h)-(l)+1))
#define SETFIELD(num, h, l, val) \
    ((num) = ((num) & ~BITMASK(h, l)) | (((val) << (l)) & BITMASK(h, l)))

int main() {
    unsigned int num = 0xABCD;
    
    printf("Original: 0x%X\n", num);
    
    // Set bits 0-3
    SETBITS(num, 0x0F);
    printf("After SETBITS(0x0F): 0x%X\n", num);
    
    // Extract bits 8-11
    printf("GETFIELD(11, 8): 0x%X\n", GETFIELD(num, 11, 8));
    
    // Set bits 4-7 to 0x5
    SETFIELD(num, 7, 4, 0x5);
    printf("After SETFIELD(7, 4, 0x5): 0x%X\n", num);
    
    return 0;
}
