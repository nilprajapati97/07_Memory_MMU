/* Dangling, Wild, Void, NULL Pointers */
#include <stdio.h>
#include <stdlib.h>

void demonstrate_dangling() {
    int *ptr = (int *)malloc(sizeof(int));
    *ptr = 42;
    printf("Before free: %d\n", *ptr);
    
    free(ptr);
    // ptr is now dangling - points to freed memory
    // *ptr = 10;  // UNDEFINED BEHAVIOR
    
    ptr = NULL;  // Good practice: set to NULL after free
    printf("Dangling pointer fixed by setting to NULL\n");
}

int *return_local_address() {
    int local = 100;
    return &local;  // DANGER: Returning address of local variable
}

void demonstrate_wild() {
    int *wild_ptr;  // Uninitialized pointer (wild pointer)
    // *wild_ptr = 10;  // UNDEFINED BEHAVIOR
    
    wild_ptr = NULL;  // Initialize to NULL
    printf("Wild pointer initialized to NULL\n");
}

void demonstrate_void() {
    int x = 10;
    float y = 3.14;
    
    void *void_ptr;  // Can point to any type
    
    void_ptr = &x;
    printf("void_ptr pointing to int: %d\n", *(int *)void_ptr);
    
    void_ptr = &y;
    printf("void_ptr pointing to float: %.2f\n", *(float *)void_ptr);
}

void demonstrate_null() {
    int *null_ptr = NULL;  // Explicitly null
    
    // Always check before dereferencing
    if (null_ptr != NULL) {
        printf("Value: %d\n", *null_ptr);
    } else {
        printf("NULL pointer - cannot dereference\n");
    }
    
    // NULL is typically 0
    printf("NULL value: %p\n", (void *)null_ptr);
}

int main() {
    printf("=== Dangling Pointer ===\n");
    demonstrate_dangling();
    
    printf("\n=== Wild Pointer ===\n");
    demonstrate_wild();
    
    printf("\n=== Void Pointer ===\n");
    demonstrate_void();
    
    printf("\n=== NULL Pointer ===\n");
    demonstrate_null();
    
    printf("\n=== Dangling (Local Variable) ===\n");
    // int *bad_ptr = return_local_address();  // DANGER
    printf("Never return address of local variable!\n");
    
    return 0;
}
