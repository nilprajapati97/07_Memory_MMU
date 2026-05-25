/* Pointer vs Array Declarations */
#include <stdio.h>

int main() {
    // 1. Pointer to array of 10 integers
    int arr1[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int (*ptr_to_array)[10] = &arr1;
    
    printf("Pointer to array of 10 integers:\n");
    printf("(*ptr_to_array)[0] = %d\n", (*ptr_to_array)[0]);
    printf("(*ptr_to_array)[5] = %d\n", (*ptr_to_array)[5]);
    
    // 2. Array of 10 integer pointers
    int a = 1, b = 2, c = 3;
    int *arr_of_ptrs[10];
    arr_of_ptrs[0] = &a;
    arr_of_ptrs[1] = &b;
    arr_of_ptrs[2] = &c;
    
    printf("\nArray of 10 integer pointers:\n");
    printf("*arr_of_ptrs[0] = %d\n", *arr_of_ptrs[0]);
    printf("*arr_of_ptrs[1] = %d\n", *arr_of_ptrs[1]);
    printf("*arr_of_ptrs[2] = %d\n", *arr_of_ptrs[2]);
    
    // Size difference
    printf("\nSize comparison:\n");
    printf("sizeof(ptr_to_array) = %zu (pointer size)\n", sizeof(ptr_to_array));
    printf("sizeof(arr_of_ptrs) = %zu (10 pointers)\n", sizeof(arr_of_ptrs));
    
    return 0;
}
