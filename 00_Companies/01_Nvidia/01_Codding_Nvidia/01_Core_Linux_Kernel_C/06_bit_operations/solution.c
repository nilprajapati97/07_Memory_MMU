// Bit operations: set, clear, test bit
#include <stdio.h>
#include <stdbool.h>

void set_bit(unsigned int *num, int bit) {
    *num |= (1U << bit);
}

void clear_bit(unsigned int *num, int bit) {
    *num &= ~(1U << bit);
}

bool test_bit(unsigned int num, int bit) {
    return (num & (1U << bit)) != 0;
}
