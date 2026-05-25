/*
 * File   : rb_bitwise_shift.c
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
 * Approach: Bitwise Shift (Simple)
 *   - Iterate over all 32 bits of n.
 *   - Pick the least significant bit of n and append it
 *     to the result by left-shifting result and OR-ing the bit.
 *   - Right-shift n to process the next bit.
 *
 * Complexity:
 *   Time  : O(32) = O(1)
 *   Space : O(1)
 */
#include <stdio.h>

unsigned int reverse_bits(unsigned int n) {
    unsigned int result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (n & 1);
        n >>= 1;
    }
    return result;
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
