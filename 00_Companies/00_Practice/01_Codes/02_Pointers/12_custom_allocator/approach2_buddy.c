/* Approach 2: Buddy Allocator */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MIN_BLOCK_SIZE 32
#define MAX_BLOCK_SIZE 1024
#define HEAP_SIZE 2048

static char heap[HEAP_SIZE];

typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;

static block_t *free_lists[6];  // For sizes 32, 64, 128, 256, 512, 1024

static int initialized = 0;

int get_level(size_t size) {
    int level = 0;
    size_t block_size = MIN_BLOCK_SIZE;
    
    while (block_size < size && block_size < MAX_BLOCK_SIZE) {
        block_size *= 2;
        level++;
    }
    
    return level;
}

void init_allocator() {
    if (initialized) return;
    
    // Initialize with one large block
    block_t *block = (block_t *)heap;
    block->size = MAX_BLOCK_SIZE;
    block->is_free = 1;
    block->next = NULL;
    
    free_lists[5] = block;  // Level 5 = 1024 bytes
    
    initialized = 1;
}

void *buddy_malloc(size_t size) {
    if (!initialized) init_allocator();
    
    size += sizeof(block_t);
    int level = get_level(size);
    
    if (level >= 6) return NULL;
    
    // Find free block
    for (int i = level; i < 6; i++) {
        if (free_lists[i]) {
            block_t *block = free_lists[i];
            free_lists[i] = block->next;
            
            // Split if necessary
            while (i > level) {
                i--;
                size_t buddy_size = MIN_BLOCK_SIZE << i;
                block_t *buddy = (block_t *)((char *)block + buddy_size);
                buddy->size = buddy_size;
                buddy->is_free = 1;
                buddy->next = free_lists[i];
                free_lists[i] = buddy;
            }
            
            block->is_free = 0;
            return (void *)(block + 1);
        }
    }
    
    return NULL;
}

void buddy_free(void *ptr) {
    if (!ptr) return;
    
    block_t *block = (block_t *)ptr - 1;
    block->is_free = 1;
    
    int level = get_level(block->size);
    block->next = free_lists[level];
    free_lists[level] = block;
}

int main() {
    printf("Buddy Allocator Demo\n\n");
    
    void *p1 = buddy_malloc(50);
    printf("Allocated 50 bytes: %p\n", p1);
    
    void *p2 = buddy_malloc(100);
    printf("Allocated 100 bytes: %p\n", p2);
    
    void *p3 = buddy_malloc(200);
    printf("Allocated 200 bytes: %p\n", p3);
    
    buddy_free(p2);
    printf("\nFreed p2\n");
    
    void *p4 = buddy_malloc(80);
    printf("Allocated 80 bytes: %p\n", p4);
    
    return 0;
}
