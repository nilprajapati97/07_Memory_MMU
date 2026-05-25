/* Approach 3: Inline Functions (Type-safe) */
#include <stdio.h>

static inline void set_bit(unsigned int *num, int n) {
    *num |= (1U << n);
}

static inline void clear_bit(unsigned int *num, int n) {
    *num &= ~(1U << n);
}

static inline void toggle_bit(unsigned int *num, int n) {
    *num ^= (1U << n);
}

static inline int check_bit(unsigned int num, int n) {
    return (num >> n) & 1;
}

// Multi-bit operations
static inline unsigned int get_field(unsigned int num, int high, int low) {
    int width = high - low + 1;
    unsigned int mask = (1U << width) - 1;
    return (num >> low) & mask;
}

static inline void set_field(unsigned int *num, int high, int low, unsigned int val) {
    int width = high - low + 1;
    unsigned int mask = ((1U << width) - 1) << low;
    *num = (*num & ~mask) | ((val << low) & mask);
}

int main() {
    unsigned int num = 20;
    
    printf("Original: %u\n", num);
    
    set_bit(&num, 0);
    printf("After set_bit(0): %u\n", num);
    
    clear_bit(&num, 2);
    printf("After clear_bit(2): %u\n", num);
    
    toggle_bit(&num, 3);
    printf("After toggle_bit(3): %u\n", num);
    
    printf("check_bit(4): %d\n", check_bit(num, 4));
    
    // Field operations
    unsigned int reg = 0xABCD;
    printf("\nRegister: 0x%X\n", reg);
    printf("get_field(11, 8): 0x%X\n", get_field(reg, 11, 8));
    
    set_field(&reg, 7, 4, 0x5);
    printf("After set_field(7, 4, 0x5): 0x%X\n", reg);
    
    return 0;
}
