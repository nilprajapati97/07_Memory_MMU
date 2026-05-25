/* Approach 1: Demonstration with Examples */
#include <stdio.h>

int main() {
    int x = 10, y = 20;
    
    // 1. const int *p - Pointer to constant int
    // Can change pointer, cannot change value
    const int *p1 = &x;
    printf("const int *p1 = %d\n", *p1);
    // *p1 = 30;  // ERROR: Cannot modify value
    p1 = &y;      // OK: Can change pointer
    printf("After p1 = &y: %d\n", *p1);
    
    // 2. int * const p - Constant pointer to int
    // Cannot change pointer, can change value
    int * const p2 = &x;
    printf("\nint * const p2 = %d\n", *p2);
    *p2 = 30;     // OK: Can modify value
    printf("After *p2 = 30: %d\n", *p2);
    // p2 = &y;   // ERROR: Cannot change pointer
    
    // 3. const int * const p - Constant pointer to constant int
    // Cannot change pointer, cannot change value
    const int * const p3 = &x;
    printf("\nconst int * const p3 = %d\n", *p3);
    // *p3 = 40;  // ERROR: Cannot modify value
    // p3 = &y;   // ERROR: Cannot change pointer
    
    return 0;
}
