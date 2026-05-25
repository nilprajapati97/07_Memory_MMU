/* Approach 3: Pool Allocator (Fixed Size) */
#include <stdio.h>
#include <stddef.h>

#define POOL_SIZE 10
#define BLOCK_SIZE 64

typedef struct free_block {
    struct free_block *next;
} free_block_t;

typedef struct {
    char memory[POOL_SIZE * BLOCK_SIZE];
    free_block_t *free_list;
    int allocated_count;
} pool_t;

void pool_init(pool_t *pool) {
    pool->free_list = NULL;
    pool->allocated_count = 0;
    
    // Link all blocks
    for (int i = 0; i < POOL_SIZE; i++) {
        free_block_t *block = (free_block_t *)(pool->memory + i * BLOCK_SIZE);
        block->next = pool->free_list;
        pool->free_list = block;
    }
}

void *pool_alloc(pool_t *pool) {
    if (!pool->free_list) {
        return NULL;  // Pool exhausted
    }
    
    free_block_t *block = pool->free_list;
    pool->free_list = block->next;
    pool->allocated_count++;
    
    return (void *)block;
}

void pool_free(pool_t *pool, void *ptr) {
    if (!ptr) return;
    
    free_block_t *block = (free_block_t *)ptr;
    block->next = pool->free_list;
    pool->free_list = block;
    pool->allocated_count--;
}

void pool_stats(pool_t *pool) {
    printf("\nPool Statistics:\n");
    printf("Total blocks: %d\n", POOL_SIZE);
    printf("Block size: %d bytes\n", BLOCK_SIZE);
    printf("Allocated: %d\n", pool->allocated_count);
    printf("Free: %d\n", POOL_SIZE - pool->allocated_count);
}

int main() {
    pool_t pool;
    pool_init(&pool);
    
    printf("Pool Allocator Demo\n");
    pool_stats(&pool);
    
    // Allocate some blocks
    void *p1 = pool_alloc(&pool);
    void *p2 = pool_alloc(&pool);
    void *p3 = pool_alloc(&pool);
    
    printf("\nAfter allocating 3 blocks:\n");
    printf("p1: %p\n", p1);
    printf("p2: %p\n", p2);
    printf("p3: %p\n", p3);
    pool_stats(&pool);
    
    // Free one block
    pool_free(&pool, p2);
    printf("\nAfter freeing p2:\n");
    pool_stats(&pool);
    
    // Allocate again (should reuse p2)
    void *p4 = pool_alloc(&pool);
    printf("\nAllocated p4: %p (should be same as p2)\n", p4);
    pool_stats(&pool);
    
    return 0;
}
