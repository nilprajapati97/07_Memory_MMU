/* malloc, calloc, realloc, free - Comparison */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void demonstrate_malloc() {
    printf("=== malloc ===\n");
    int *arr = (int *)malloc(5 * sizeof(int));
    if (!arr) {
        printf("malloc failed\n");
        return;
    }
    
    // malloc doesn't initialize - contains garbage
    printf("malloc (uninitialized): ");
    for (int i = 0; i < 5; i++) {
        arr[i] = i + 1;  // Must initialize manually
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    free(arr);
}

void demonstrate_calloc() {
    printf("\n=== calloc ===\n");
    int *arr = (int *)calloc(5, sizeof(int));
    if (!arr) {
        printf("calloc failed\n");
        return;
    }
    
    // calloc initializes to zero
    printf("calloc (zero-initialized): ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    free(arr);
}

void demonstrate_realloc() {
    printf("\n=== realloc ===\n");
    int *arr = (int *)malloc(3 * sizeof(int));
    if (!arr) return;
    
    for (int i = 0; i < 3; i++) {
        arr[i] = i + 1;
    }
    
    printf("Original (3 elements): ");
    for (int i = 0; i < 3; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    // Resize to 6 elements
    int *new_arr = (int *)realloc(arr, 6 * sizeof(int));
    if (!new_arr) {
        free(arr);
        return;
    }
    arr = new_arr;
    
    // Initialize new elements
    for (int i = 3; i < 6; i++) {
        arr[i] = i + 1;
    }
    
    printf("After realloc (6 elements): ");
    for (int i = 0; i < 6; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    
    free(arr);
}

int main() {
    demonstrate_malloc();
    demonstrate_calloc();
    demonstrate_realloc();
    
    printf("\n=== Comparison ===\n");
    printf("malloc:  Allocates memory, NO initialization\n");
    printf("calloc:  Allocates memory, ZERO initialization\n");
    printf("realloc: Resizes memory, preserves existing data\n");
    printf("free:    Deallocates memory\n");
    
    return 0;
}
