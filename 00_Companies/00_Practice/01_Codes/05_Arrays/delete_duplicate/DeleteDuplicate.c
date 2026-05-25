/*
 * File   : Delete_Duplicate.c
 * Topic  : Delete Duplicate Elements from an Array
 * Approach: Approach 1 — Brute Force (Nested Loop)
 *
 * Description:
 *   For every element in the array, scan all previously accepted unique
 *   elements to decide if the current element is a duplicate.
 *   If it is NOT a duplicate, it is kept; otherwise it is skipped.
 *
 * Algorithm Steps:
 *   1. Maintain a counter 'k' for the number of unique elements found so far.
 *   2. For each element arr[i] (outer loop):
 *        a. Check arr[i] against arr[0..k-1] (inner loop).
 *        b. If a match is found -> mark as duplicate and break.
 *        c. If no match -> copy arr[i] to arr[k] and increment k.
 *   3. Return k as the new array size.
 *
 * Complexity:
 *   Time  -> O(n^2)  [nested loop: each element checked against kept elements]
 *   Space -> O(1)    [in-place, no extra memory used]
 *
 * Order Preserved: YES
 *
 * Related Files:
 *   Approach 2 -> Delete_Duplicate_Optimized.c  [O(n log n), order NOT preserved]
 *   Approach 3 -> Delete_Duplicate_HashSet.c    [O(n) avg,   order preserved]
 */

#include <stdio.h>

/*
 * removeDuplicates:
 *   Removes duplicate values from arr[] of size n using a nested loop.
 *   Unique elements are compacted to the front of arr[].
 *   Returns the count of unique elements (new logical size).
 */
int removeDuplicates(int arr[], int n) {
    int k = 0; /* count of unique elements accepted so far */

    for (int i = 0; i < n; i++) {
        int isDuplicate = 0;

        /* Inner loop: check if arr[i] already exists in arr[0..k-1] */
        for (int j = 0; j < k; j++) {
            if (arr[j] == arr[i]) {
                isDuplicate = 1; /* duplicate found — stop searching */
                break;
            }
        }

        /* If unique, place it at position k and advance k */
        if (!isDuplicate) {
            arr[k] = arr[i];
            k++;
        }
    }

    return k; /* new size of array with duplicates removed */
}

int main(void) {
    int arr[100]; /* input array — supports up to 100 elements */
    int n;        /* number of elements entered by user */

    printf("Enter number of elements: ");
    if (scanf("%d", &n) != 1 || n <= 0 || n > 100) {
        printf("Invalid input.\n");
        return 1;
    }

    printf("Enter %d elements: ", n);
    for (int i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }

    /* Remove duplicates and get the new array size */
    int newSize = removeDuplicates(arr, n);

    /* Print the result — only the first newSize elements are valid */
    printf("Array after removing duplicates: ");
    for (int i = 0; i < newSize; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}