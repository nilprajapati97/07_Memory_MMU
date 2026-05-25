/* GCC Builtin - Production Code Choice
 * Time: O(1) - uses hardware POPCNT instruction
 * Space: O(1)
 * 
 * Note: Most efficient on modern CPUs with POPCNT support
 * Falls back to optimized software implementation if not available
 */
#include <stdio.h>

int countSetBits(int n) {
    return __builtin_popcount(n);
}

int main(void) {
    int num;
    printf("Enter number: ");
    scanf("%d", &num);
    printf("Set bits: %d\n", countSetBits(num));
    return 0;
}
