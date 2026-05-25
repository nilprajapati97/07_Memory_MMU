/* Approach 3: In-place Modification */
#include <stdio.h>

void set_bit(unsigned int *num, int n) {
    *num |= (1U << n);
}

void clear_bit(unsigned int *num, int n) {
    *num &= ~(1U << n);
}

void toggle_bit(unsigned int *num, int n) {
    *num ^= (1U << n);
}

int check_bit(unsigned int num, int n) {
    return (num >> n) & 1;
}

int main() {
    unsigned int num = 20; // 10100
    
    printf("Original: %u\n", num);
    
    set_bit(&num, 0);
    printf("After set bit 0: %u\n", num);
    
    clear_bit(&num, 2);
    printf("After clear bit 2: %u\n", num);
    
    toggle_bit(&num, 3);
    printf("After toggle bit 3: %u\n", num);
    
    printf("Check bit 4: %d\n", check_bit(num, 4));
    
    return 0;
}
