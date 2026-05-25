/* Approach 1: XOR (Best) */
#include <stdio.h>

int find_single(int arr[], int n) {
    int result = 0;
    for (int i = 0; i < n; i++)
        result ^= arr[i];
    return result;
}

int main() {
    int arr[] = {2, 3, 5, 4, 5, 3, 4};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    printf("Single number: %d\n", find_single(arr, n));
    return 0;
}
