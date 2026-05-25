/* Recursive Approach
 * Time: O(log n) - number of bits
 * Space: O(log n) - recursion stack depth
 * 
 * Logic: Check LSB (n & 1), recurse on remaining bits (n >> 1)
 * Example: 13 (1101) → 1 + recurse(6) → 1 + 0 + recurse(3) → ...
 */
#include <stdio.h>

int countSetBits(int n) {
    if (n == 0)
        return 0;
    return (n & 1) + countSetBits(n >> 1);
}

int main(void) {
    int num;
    printf("Enter number: ");
    scanf("%d", &num);
    printf("Set bits: %d\n", countSetBits(num));
    return 0;
}
