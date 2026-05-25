// Bit Manipulation Utilities
#include <stdint.h>
#include <stdio.h>

// Count set bits in a 64-bit integer (Hamming weight)
unsigned int count_set_bits(uint64_t n) {
    unsigned int count = 0;
    unsigned int instructions = 0;
    // Simulate a low memory threshold for demonstration
    const unsigned int LOW_MEMORY_THRESHOLD = 2; // e.g., warn if only 2 bits set
    while (n) {
        n &= (n - 1); // Clear the least significant set bit
        count++;
        instructions++;
    }
    // Log for optimization: number of instructions (iterations)
    printf("[LOG][Optimization] count_set_bits executed %u instructions.\n", instructions);
    // Log for low memory: warn if set bits are below threshold
    if (count <= LOW_MEMORY_THRESHOLD) {
        printf("[LOG][LowMemory] Warning: Only %u set bits detected (possible low memory usage).\n", count);
    }
    return count;
}

// Check if a memory address is aligned to a 4KB page boundary
int is_aligned_4kb(void *addr) {
    return (((uintptr_t)addr & 0xFFF) == 0);
}

// Demo usage
int main() {
    uint64_t val = 0xF0F0F0F0F0F0F0F0ULL;
    printf("Set bits in %llx: %u\n", val, count_set_bits(val));

    void *addr1 = (void*)0x1000;
    void *addr2 = (void*)0x1234;
    printf("Address %p aligned to 4KB? %s\n", addr1, is_aligned_4kb(addr1) ? "Yes" : "No");
    printf("Address %p aligned to 4KB? %s\n", addr2, is_aligned_4kb(addr2) ? "Yes" : "No");
    return 0;
}