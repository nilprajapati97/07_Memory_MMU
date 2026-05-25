/*
 * File   : rb_lookup_table.c
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
 * Approach: Lookup Table
 *   - Precompute the reverse of every 8-bit value (0..255) once.
 *   - Split the 32-bit number into 4 bytes.
 *   - Look up the reversed byte for each and place it in the
 *     mirrored position to build the final reversed integer.
 *
 * Complexity:
 *   Time  : O(1)  per query (after O(256) preprocessing)
 *   Space : O(256)
 */
#include <stdio.h>

unsigned char reverse_byte[256];

void init_lookup() {
    for (int i = 0; i < 256; i++) {
        unsigned char rev = 0;
        unsigned char num = i;
        for (int j = 0; j < 8; j++) {
            rev = (rev << 1) | (num & 1);
            num >>= 1;
        }
        reverse_byte[i] = rev;
    }
}

unsigned int reverse_bits(unsigned int n) {
    return (reverse_byte[n & 0xFF] << 24) |
           (reverse_byte[(n >> 8) & 0xFF] << 16) |
           (reverse_byte[(n >> 16) & 0xFF] << 8) |
           (reverse_byte[(n >> 24) & 0xFF]);
}

int main() {
    init_lookup();
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
