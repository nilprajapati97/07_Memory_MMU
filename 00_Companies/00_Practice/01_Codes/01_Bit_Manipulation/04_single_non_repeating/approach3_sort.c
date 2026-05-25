/* Approach 3: Sorting */
#include <stdio.h>
#include <stdlib.h>

int compare(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

int find_single(int arr[], int n) {
    qsort(arr, n, sizeof(int), compare);
    
    for (int i = 0; i < n - 1; i += 2) {
        if (arr[i] != arr[i + 1])
            return arr[i];
    }
    return arr[n - 1];
}

int main() {
    int arr[] = {2, 3, 5, 4, 5, 3, 4};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    printf("Single number: %d\n", find_single(arr, n));
    return 0;
}
