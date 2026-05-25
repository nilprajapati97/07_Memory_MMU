#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

// Define a block structure for memory management
struct Block {
    size_t size;          // Size of the block
    int free;             // Free flag (1 if free, 0 if allocated)
    struct Block *next;   // Pointer to the next block
};

#define BLOCK_SIZE sizeof(struct Block)

// Head of the linked list of memory blocks
static struct Block *head = NULL;

// Function to find a free block of sufficient size
static struct Block *find_free_block(size_t size) {
    struct Block *current = head;
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Function to request memory from the OS
static struct Block *request_memory(size_t size) {
    struct Block *block = mmap(NULL, size + BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) {
        return NULL;
    }
    block->size = size;
    block->free = 0;
    block->next = NULL;
    return block;
}

// Custom malloc implementation
void *my_malloc(size_t size) {
    if (size <= 0) {
        return NULL;
    }

    struct Block *block = find_free_block(size);
    if (!block) {
        block = request_memory(size);
        if (!block) {
            return NULL;
        }
        if (!head) {
            head = block;
        } else {
            struct Block *current = head;
            while (current->next) {
                current = current->next;
            }
            current->next = block;
        }
    } else {
        block->free = 0;
    }

    return (void *)(block + 1);
}

// Custom free implementation
void my_free(void *ptr) {
    if (!ptr) {
        return;
    }

    struct Block *block = (struct Block *)ptr - 1;
    block->free = 1;
}

// Custom realloc implementation
void *my_realloc(void *ptr, size_t size) {
    if (!ptr) {
        return my_malloc(size);
    }

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    struct Block *block = (struct Block *)ptr - 1;
    if (block->size >= size) {
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        my_free(ptr);
    }

    return new_ptr;
}

// Test the custom malloc implementation
int main() {
    void *ptr1 = my_malloc(100);
    void *ptr2 = my_malloc(200);
    my_free(ptr1);
    void *ptr3 = my_malloc(50);
    ptr2 = my_realloc(ptr2, 300);

    printf("Custom malloc test completed.\n");
    return 0;
}