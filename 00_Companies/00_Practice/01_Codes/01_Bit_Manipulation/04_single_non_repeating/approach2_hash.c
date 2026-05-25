/* Approach 2: Hash Map */
#include <stdio.h>
#include <stdlib.h>

#define MAX 1000

int find_single(int arr[], int n) {
    int *hash = calloc(MAX, sizeof(int));
    
    for (int i = 0; i < n; i++)
        hash[arr[i]]++;
    
    for (int i = 0; i < MAX; i++)
        if (hash[i] == 1) {
            free(hash);
            return i;
        }
    
    free(hash);
    return -1;
}

int main() {
    int arr[] = {2, 3, 5, 4, 5, 3, 4};
    int n = sizeof(arr) / sizeof(arr[0]);
    
    printf("Single number: %d\n", find_single(arr, n));
    return 0;
}
