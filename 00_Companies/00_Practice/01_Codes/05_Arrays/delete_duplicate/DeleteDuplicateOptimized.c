/*
 * File: Delete_Duplicate_Optimized.c
 *
 * What we have done:
 * ------------------
 * Problem:
 *   Remove duplicate elements from an integer array.
 *
 * Approach 1 (Delete_Duplicate.c) — O(n^2):
 *   Used a nested loop to check every element against all previously
 *   kept unique elements. Simple but slow for large arrays.
 *
 * Approach 2 (This file) — O(n log n):
 *   Step 1: Sort the array using qsort() so all duplicate values
 *           become adjacent to each other.
 *   Step 2: Traverse the sorted array once using a two-pointer
 *           technique. 'k' tracks the next position for a unique
 *           element. If the current element differs from the last
 *           kept element, copy it to arr[k] and increment k.
 *
 * Complexity:
 *   Time  -> O(n log n)  [sorting dominates]
 *   Space -> O(1)        [in-place, no extra array needed]
 *
 * Trade-off:
 *   Original order is NOT preserved because the array is sorted first.
 *   If order must be preserved, a hash-set approach (O(n) avg) is needed.
 */

#include <stdio.h>
#include <stdlib.h>

/* Comparator for qsort — avoids subtraction to prevent integer overflow */
int compare(const void *a, const void *b) {
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

/*
 * removeDuplicates:
 *   Sorts arr[] then removes adjacent duplicates in a single pass.
 *   Returns the new size (number of unique elements).
 *   The unique elements are stored in arr[0..newSize-1].
 */
int removeDuplicates(int arr[], int n) {
    if (n <= 1) return n;

    /* Step 1: Sort so duplicates become adjacent — O(n log n) */
    qsort(arr, n, sizeof(int), compare);

    /* Step 2: Two-pointer scan to collapse duplicates — O(n) */
    int k = 1; /* arr[0] is always unique; k starts at 1 */
    for (int i = 1; i < n; i++) {
        if (arr[i] != arr[k - 1]) {  /* new unique element found */
            arr[k] = arr[i];
            k++;
        }
        /* if arr[i] == arr[k-1], it's a duplicate — skip it */
    }

    return k;
}

int main(void) {
    int arr[100];
    int n;

    printf("Enter number of elements: ");
    if (scanf("%d", &n) != 1 || n <= 0 || n > 100) {
        printf("Invalid input.\n");
        return 1;
    }

    printf("Enter %d elements: ", n);
    for (int i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }

    int newSize = removeDuplicates(arr, n);

    printf("Array after removing duplicates: ");
    for (int i = 0; i < newSize; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}
