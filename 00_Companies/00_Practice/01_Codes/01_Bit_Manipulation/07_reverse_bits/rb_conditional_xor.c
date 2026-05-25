/*
 * File   : rb_conditional_xor.c
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
 * Approach: Conditional XOR (Optimized Two Pointer)
 *   - for loop with two indices: i (LSB side) and j (MSB side) moving inward.
 *   - If bit[i] == bit[j] → already mirrored, skip.
 *   - If bit[i] != bit[j] → XOR both positions to flip them in one step.
 *   - Loop runs only 16 times (half of 32).
 *
 * Complexity:
 *   Time  : O(16) = O(1)   — half iterations vs simple loop
 *   Space : O(1)
 */
#include <stdio.h>

unsigned int reverse_bits(unsigned int n) {
    for (int i = 0, j = 31; i < j; i++, j--) {
        int bit_i = (n >> i) & 1;
        int bit_j = (n >> j) & 1;
        if (bit_i != bit_j)             /* skip if already mirrored */
            n ^= (1u << i) | (1u << j); /* flip both bits in one XOR */
    }
    return n;
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
