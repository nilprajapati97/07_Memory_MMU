/* Brian Kernighan's Algorithm - Best Interview Answer
 * Time: O(k) where k = number of set bits
 * Space: O(1)
 * 
 * Key Insight: n & (n-1) clears the rightmost set bit
 * Example: n=12 (1100) → n-1=11 (1011) → n&(n-1)=8 (1000)
 */
#include <stdio.h>

int countSetBits(int n) {
    int count = 0;
    while (n) {
        n &= (n - 1);  // Clear rightmost set bit
        count++;
    }
    return count;
}

int main(void) {
    int num;
    printf("Enter number: ");
    scanf("%d", &num);
    printf("Set bits in %d: %d\n", num, countSetBits(num));
    return 0;
}
