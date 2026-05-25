/* Approach 1: XOR with Bit Partitioning (Best) */
#include <stdio.h>

void find_two(int arr[], int n, int *x, int *y) {
    int xor = 0;
    for (int i = 0; i < n; i++)
        xor ^= arr[i];
    
    int rightmost_set = xor & -xor;
    *x = *y = 0;
    
    for (int i = 0; i < n; i++) {
        if (arr[i] & rightmost_set)
            *x ^= arr[i];
        else
            *y ^= arr[i];
    }
}

int main() {
    int arr[] = {2, 3, 7, 9, 11, 2, 3, 11};
    int n = sizeof(arr) / sizeof(arr[0]);
    int x, y;
    
    find_two(arr, n, &x, &y);
    printf("Two non-repeating: %d and %d\n", x, y);
    return 0;
}
