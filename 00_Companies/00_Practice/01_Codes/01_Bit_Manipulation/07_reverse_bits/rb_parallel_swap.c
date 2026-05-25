/*
 * File   : rb_parallel_swap.c
 * Author : Nil Prajapati
 * Topic  : Bit Manipulation - Reverse Bits
 *
 * Problem:
 *   Reverse the bits of a given 32-bit unsigned integer.
 *
 *   Example:
 *     Input  : n = 43261596            (00000010100101000001111010011100)
 *     Output : 964176192               (00111001011110000010100101000000)
 *
 * Approach: Parallel Swap (Divide and Conquer)
 *   - Swap bits in parallel using bit masks.
 *   - Step 1: Swap the two 16-bit halves.
 *   - Step 2: Within each 16-bit half, swap the two 8-bit halves.
 *   - Step 3: Swap adjacent 4-bit nibbles.
 *   - Step 4: Swap adjacent 2-bit pairs.
 *   - Step 5: Swap adjacent single bits.
 *
 * Complexity:
 *   Time  : O(log 32) = O(1)
 *   Space : O(1)
 */
#include <stdio.h>

unsigned int reverse_bits(unsigned int n) {
    n = ((n & 0xFFFF0000) >> 16) | ((n & 0x0000FFFF) << 16);
    n = ((n & 0xFF00FF00) >> 8)  | ((n & 0x00FF00FF) << 8);
    n = ((n & 0xF0F0F0F0) >> 4)  | ((n & 0x0F0F0F0F) << 4);
    n = ((n & 0xCCCCCCCC) >> 2)  | ((n & 0x33333333) << 2);
    n = ((n & 0xAAAAAAAA) >> 1)  | ((n & 0x55555555) << 1);
    return n;
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
