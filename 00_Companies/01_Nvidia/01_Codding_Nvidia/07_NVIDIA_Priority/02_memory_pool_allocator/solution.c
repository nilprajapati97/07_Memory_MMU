// Simple memory pool allocator in C
#include <stdlib.h>
#include <stdint.h>
#define POOL_SIZE 1024

static uint8_t pool[POOL_SIZE];
static size_t offset = 0;

void *pool_alloc(size_t size) {
    if (offset + size > POOL_SIZE) return NULL;
    void *ptr = &pool[offset];
    offset += size;
    return ptr;
}

void pool_reset() {
    offset = 0;
}
