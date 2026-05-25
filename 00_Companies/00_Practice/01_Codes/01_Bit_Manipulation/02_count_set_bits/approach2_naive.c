/* Naive Approach - Check Each Bit
 * Time: O(log n) or O(32) for 32-bit integer
 * Space: O(1)
 * 
 * Logic: Check LSB with (n & 1), then right shift
 * Example: 13 (1101) → check 1, shift → 6 (110) → check 0, shift...
 */
#include <stdio.h>

int countSetBits(int n) {
    int count = 0;
    while (n) {
        count += (n & 1);  // Check if LSB is 1
        n >>= 1;           // Right shift by 1
    }
    return count;
}

int main(void) {
    int num;
    printf("Enter number: ");
    scanf("%d", &num);
    printf("Set bits: %d\n", countSetBits(num));
    return 0;
}
