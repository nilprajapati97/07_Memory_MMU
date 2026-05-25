/* Approach 1: Simple Custom Allocator */
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define HEAP_SIZE 1024

// Simple heap
static char heap[HEAP_SIZE];
static size_t heap_top = 0;

// Block header
typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;

static block_t *free_list = NULL;

void *my_malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    // Search free list
    block_t *curr = free_list;
    block_t *prev = NULL;
    
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = 0;
            return (void *)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }
    
    // Allocate new block
    if (heap_top + sizeof(block_t) + size > HEAP_SIZE) {
        return NULL;  // Out of memory
    }
    
    block_t *block = (block_t *)(heap + heap_top);
    block->size = size;
    block->is_free = 0;
    block->next = NULL;
    
    if (prev) {
        prev->next = block;
    } else {
        free_list = block;
    }
    
    heap_top += sizeof(block_t) + size;
    
    return (void *)(block + 1);
}

void my_free(void *ptr) {
    if (!ptr) return;
    
    block_t *block = (block_t *)ptr - 1;
    block->is_free = 1;
}

void print_heap_status() {
    printf("\nHeap Status:\n");
    printf("Total size: %d bytes\n", HEAP_SIZE);
    printf("Used: %zu bytes\n", heap_top);
    printf("Free: %zu bytes\n", HEAP_SIZE - heap_top);
    
    printf("\nBlocks:\n");
    block_t *curr = free_list;
    int i = 0;
    while (curr) {
        printf("Block %d: size=%zu, %s\n", i++, curr->size, 
               curr->is_free ? "FREE" : "USED");
        curr = curr->next;
    }
}

int main() {
    printf("Custom Memory Allocator Demo\n");
    
    // Allocate some memory
    int *arr1 = (int *)my_malloc(5 * sizeof(int));
    printf("Allocated arr1: %p\n", (void *)arr1);
    
    char *str = (char *)my_malloc(20);
    printf("Allocated str: %p\n", (void *)str);
    
    int *arr2 = (int *)my_malloc(10 * sizeof(int));
    printf("Allocated arr2: %p\n", (void *)arr2);
    
    print_heap_status();
    
    // Free some memory
    my_free(str);
    printf("\nAfter freeing str:\n");
    print_heap_status();
    
    // Allocate again (should reuse freed block)
    char *str2 = (char *)my_malloc(15);
    printf("\nAllocated str2: %p\n", (void *)str2);
    print_heap_status();
    
    return 0;
}
