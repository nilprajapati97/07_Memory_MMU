/* Approach 2: Function Parameters */
#include <stdio.h>

// Function with pointer to const - cannot modify data
void print_value(const int *p) {
    printf("Value: %d\n", *p);
    // *p = 100;  // ERROR: Cannot modify
}

// Function with const pointer - cannot change pointer
void modify_value(int * const p) {
    *p = 100;  // OK: Can modify value
    // p++;     // ERROR: Cannot change pointer
}

// Function with const pointer to const
void read_only(const int * const p) {
    printf("Read-only: %d\n", *p);
    // *p = 100;  // ERROR
    // p++;       // ERROR
}

int main() {
    int arr[] = {1, 2, 3, 4, 5};
    
    print_value(&arr[0]);
    modify_value(&arr[0]);
    printf("After modify: %d\n", arr[0]);
    read_only(&arr[0]);
    
    return 0;
}
