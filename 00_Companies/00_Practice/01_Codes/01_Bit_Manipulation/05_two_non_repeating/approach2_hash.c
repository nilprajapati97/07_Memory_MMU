/* Approach 2: Hash Map */
#include <stdio.h>
#include <stdlib.h>

#define MAX 100

void find_two(int arr[], int n, int *x, int *y) {
    int *hash = calloc(MAX, sizeof(int));
    
    for (int i = 0; i < n; i++)
        hash[arr[i]]++;
    
    int found = 0;
    for (int i = 0; i < MAX && found < 2; i++) {
        if (hash[i] == 1) {
            if (found == 0) *x = i;
            else *y = i;
            found++;
        }
    }
    free(hash);
}

int main() {
    int arr[] = {2, 3, 7, 9, 11, 2, 3, 11};
    int n = sizeof(arr) / sizeof(arr[0]);
    int x, y;
    
    find_two(arr, n, &x, &y);
    printf("Two non-repeating: %d and %d\n", x, y);
    return 0;
}
