/* Approach 1: Using Functions */
#include <stdio.h>

unsigned int set_bit(unsigned int num, int n) {
    return num | (1U << n);
}

unsigned int clear_bit(unsigned int num, int n) {
    return num & ~(1U << n);
}

unsigned int toggle_bit(unsigned int num, int n) {
    return num ^ (1U << n);
}

int check_bit(unsigned int num, int n) {
    return (num >> n) & 1;
}

int main() {
    unsigned int num = 20; // 10100
    int n = 1;
    
    printf("Original: %u\n", num);
    printf("Set bit %d: %u\n", n, set_bit(num, n));
    printf("Clear bit %d: %u\n", n, clear_bit(num, n));
    printf("Toggle bit %d: %u\n", n, toggle_bit(num, n));
    printf("Check bit %d: %d\n", n, check_bit(num, n));
    
    return 0;
}
