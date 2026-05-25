// Simple bitmap allocator example
#include <stdio.h>
#include <stdbool.h>
#define BITMAP_SIZE 32

unsigned int bitmap = 0;

int alloc_bit() {
    for (int i = 0; i < BITMAP_SIZE; ++i) {
        if (!(bitmap & (1U << i))) {
            bitmap |= (1U << i);
            return i;
        }
    }
    return -1; // No free bit
}

void free_bit(int i) {
    bitmap &= ~(1U << i);
}
