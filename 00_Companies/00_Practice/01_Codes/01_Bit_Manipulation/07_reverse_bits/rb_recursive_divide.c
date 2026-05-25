/*
 * File   : rb_recursive_divide.c
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
 * Approach: Recursive Divide and Conquer
 *   - Split the number into two halves of equal width.
 *   - Recursively reverse each half.
 *   - Combine by placing the reversed lower half in the upper
 *     position and the reversed upper half in the lower position.
 *   - Base case: width == 1 returns the bit as is.
 *
 * Complexity:
 *   Time  : O(log 32) = O(1)
 *   Space : O(log 32) recursion stack
 */
#include <stdio.h>
#include <stdint.h>

static uint32_t reverse_helper(uint32_t n, int width) {
    if (width == 1) {
        return n & 1U;
    }
    int half = width / 2;
    uint32_t mask = (half == 32) ? 0xFFFFFFFFU : ((1U << half) - 1U);
    uint32_t low  = n & mask;
    uint32_t high = (n >> half) & mask;
    return (reverse_helper(low, half) << half) | reverse_helper(high, half);
}

unsigned int reverse_bits(unsigned int n) {
    return reverse_helper(n, 32);
}

int main() {
    unsigned int num = 43261596;
    printf("Original: %u\n", num);
    printf("Reversed: %u\n", reverse_bits(num));
    return 0;
}
