/* Lookup Table Approach - Fastest for Repeated Calls
 * Time: O(1) - constant 4 lookups for 32-bit integer
 * Space: O(256) for lookup table
 * 
 * Logic: Precompute counts for 0-255, split 32-bit into 4 bytes
 * Example: 0x12345678 → lookup[0x78] + lookup[0x56] + lookup[0x34] + lookup[0x12]
 */
#include <stdio.h>

int table[256];

void initTable() {
    for (int i = 0; i < 256; i++)
        table[i] = (i & 1) + table[i >> 1];  // Recursive formula
}

int countSetBits(int n) {
    return table[n & 0xff] +           // Byte 0
           table[(n >> 8) & 0xff] +    // Byte 1
           table[(n >> 16) & 0xff] +   // Byte 2
           table[(n >> 24) & 0xff];    // Byte 3
}

int main(void) {
    initTable();
    int num;
    printf("Enter number: ");
    scanf("%d", &num);
    printf("Set bits: %d\n", countSetBits(num));
    return 0;
}
