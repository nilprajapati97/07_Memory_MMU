/* Approach 2: Using Macros (Preferred in Embedded) */
#include <stdio.h>

#define SET_BIT(num, n)    ((num) | (1U << (n)))
#define CLEAR_BIT(num, n)  ((num) & ~(1U << (n)))
#define TOGGLE_BIT(num, n) ((num) ^ (1U << (n)))
#define CHECK_BIT(num, n)  (((num) >> (n)) & 1U)

int main() {
    unsigned int num = 20; // 10100
    int n = 1;
    
    printf("Original: %u\n", num);
    printf("Set bit %d: %u\n", n, SET_BIT(num, n));
    printf("Clear bit %d: %u\n", n, CLEAR_BIT(num, n));
    printf("Toggle bit %d: %u\n", n, TOGGLE_BIT(num, n));
    printf("Check bit %d: %u\n", n, CHECK_BIT(num, n));
    
    return 0;
}
