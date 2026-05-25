// Custom malloc/free implementation using sbrk
// Simple free list allocator with coalescing
// Not thread-safe, for educational/demo purposes only

#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define ALIGN4(size) (((size) + 3) & ~0x3)
#define BLOCK_SIZE (sizeof(BlockHeader))

typedef struct BlockHeader {
    size_t size;
    int free;
    struct BlockHeader *next;
} BlockHeader;

static BlockHeader *free_list = NULL;

// Find a free block using first-fit
static BlockHeader *find_free_block(size_t size) {
    BlockHeader *curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// Request more memory from OS
static BlockHeader *request_space(size_t size) {
    BlockHeader *block = sbrk(0);
    void *request = sbrk(size + BLOCK_SIZE);
    if (request == (void*)-1) {
        return NULL; // sbrk failed
    }
    block->size = size;
    block->free = 0;
    block->next = NULL;
    return block;
}

void *my_malloc(size_t size) {
    size = ALIGN4(size);
    if (size == 0) return NULL;

    BlockHeader *block;
    if (!free_list) {
        // First call, no free list yet
        block = request_space(size);
        if (!block) return NULL;
        free_list = block;
    } else {
        block = find_free_block(size);
        if (!block) {
            // No suitable block found, request more memory
            block = request_space(size);
            if (!block) return NULL;
            // Add to end of free list
            BlockHeader *last = free_list;
            while (last->next) last = last->next;
            last->next = block;
        } else {
            block->free = 0;
        }
    }
    return (block + 1); // Return pointer after header
}

// Coalesce adjacent free blocks
static void coalesce_free_blocks() {
    BlockHeader *curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void my_free(void *ptr) {
    if (!ptr) return;
    BlockHeader *block = (BlockHeader*)ptr - 1;
    block->free = 1;
    coalesce_free_blocks();
}

// For demonstration: print the free list
void print_free_list() {
    BlockHeader *curr = free_list;
    printf("Free list:\n");
    while (curr) {
        printf("Block %p: size=%zu, free=%d, next=%p\n", (void*)curr, curr->size, curr->free, (void*)curr->next);
        curr = curr->next;
    }
}

// Example usage
typedef struct {
    int a;
    double b;
} MyStruct;

int main() {
    printf("Custom malloc/free demo\n");
    MyStruct *p1 = (MyStruct*)my_malloc(sizeof(MyStruct));
    MyStruct *p2 = (MyStruct*)my_malloc(sizeof(MyStruct));
    my_free(p1);
    MyStruct *p3 = (MyStruct*)my_malloc(sizeof(MyStruct));
    print_free_list();
    my_free(p2);
    my_free(p3);
    print_free_list();
    return 0;
}
