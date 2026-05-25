/* Approach 2: Complex Declarations */
#include <stdio.h>

int main() {
    // Various complex declarations
    
    // 1. Pointer to array of 10 integers
    int (*p1)[10];
    int arr[10] = {0};
    p1 = &arr;
    printf("int (*p1)[10] - pointer to array of 10 ints\n");
    
    // 2. Array of 10 integer pointers
    int *p2[10];
    printf("int *p2[10] - array of 10 int pointers\n");
    
    // 3. Pointer to pointer to int
    int x = 42;
    int *px = &x;
    int **pp = &px;
    printf("int **pp = %d\n", **pp);
    
    // 4. Array of pointers to arrays
    int arr1[5] = {1, 2, 3, 4, 5};
    int arr2[5] = {6, 7, 8, 9, 10};
    int (*arr_ptrs[2])[5] = {&arr1, &arr2};
    printf("(*arr_ptrs[0])[0] = %d\n", (*arr_ptrs[0])[0]);
    
    // 5. Pointer to array of pointers
    int *ptr_arr[3] = {&x, &x, &x};
    int *(*p_to_arr_of_ptrs)[3] = &ptr_arr;
    printf("*(*p_to_arr_of_ptrs)[0] = %d\n", *(*p_to_arr_of_ptrs)[0]);
    
    return 0;
}
