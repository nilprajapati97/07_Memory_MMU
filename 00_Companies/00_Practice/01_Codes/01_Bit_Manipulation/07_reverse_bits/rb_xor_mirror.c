/*
 * File   : rb_xor_mirror.c
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
 * Approach: XOR Mirror (Two Pointer Bit Swap)
 *   - Use two indices i (from 0) and j (from 31) moving towards each other.
 *   - For each pair (i, j), if the bits at those positions differ,
 *     toggle both bits using XOR so they get swapped.
 *   - If both bits are equal, nothing needs to be done.
 *   - Stop when i >= j.
 *
 * Complexity:
 *   Time  : O(32/2) = O(1)
 *   Space : O(1)
 */
#include <stdio.h>

unsigned int reverse_bits(unsigned int n) {
    int i = 0, j = 31;
    while (i < j) {
        unsigned int bit_i = (n >> i) & 1U;
        unsigned int bit_j = (n >> j) & 1U;
        if (bit_i != bit_j) {
            n ^= (1U << i) | (1U << j);
        }
        i++;
        j--;
    }
    return n;
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
