// Simple malloc and free implementation using a static buffer
#include <stddef.h>
#define POOL_SIZE 1024
static char pool[POOL_SIZE];
static size_t offset = 0;

void *my_malloc(size_t size) {
    if (offset + size > POOL_SIZE) return NULL;
    void *ptr = &pool[offset];
    offset += size;
    return ptr;
}

void my_free(void *ptr) {
    // No-op for this simple bump allocator
}
