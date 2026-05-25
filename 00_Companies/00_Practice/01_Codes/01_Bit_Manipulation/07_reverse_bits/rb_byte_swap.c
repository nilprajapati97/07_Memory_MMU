/*
 * File   : rb_byte_swap.c
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
 * Approach: Byte Swap + Bit Reverse
 *   - First reverse the byte order using __builtin_bswap32
 *     (b0 b1 b2 b3 -> b3 b2 b1 b0).
 *   - Then, inside every byte, reverse the bits by:
 *       1. Swapping the two 4-bit nibbles.
 *       2. Swapping adjacent 2-bit pairs.
 *       3. Swapping adjacent single bits.
 *   - The combination of byte-reverse and per-byte bit-reverse
 *     gives the fully bit-reversed 32-bit value.
 *
 * Complexity:
 *   Time  : O(1)
 *   Space : O(1)
 */
#include <stdio.h>
#include <stdint.h>

unsigned int reverse_bits(unsigned int n) {
    /* Step 1: reverse byte order. */
    n = __builtin_bswap32(n);

    /* Step 2: reverse bits within every byte. */
    n = ((n & 0xF0F0F0F0U) >> 4) | ((n & 0x0F0F0F0FU) << 4);
    n = ((n & 0xCCCCCCCCU) >> 2) | ((n & 0x33333333U) << 2);
    n = ((n & 0xAAAAAAAAU) >> 1) | ((n & 0x55555555U) << 1);

    return n;
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
