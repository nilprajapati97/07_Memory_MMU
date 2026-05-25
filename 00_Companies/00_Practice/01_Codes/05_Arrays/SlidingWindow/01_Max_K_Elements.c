#include <stdio.h>
#include <stdlib.h>

/*
 * Problem: Maximum Sum of K Elements
 * 
 * Given an array of integers and a number k, find the maximum sum
 * of any k elements from the array.
 * 
 * Input: arr = [2, 1, 5, 1, 3, 2], k = 3
 * Output: 9
 * Explanation: Select elements 5, 3, 2 → sum = 10? or 5+2+2=9
 * Actually: 5 + 2 + 2 = 9 (largest k elements)
 * 
 * Approach: Sort array in descending order and sum first k elements
 * Time: O(n log n)
 * Space: O(1)
 */

int compare(const void *a, const void *b) {
    return (*(int*)b - *(int*)a);  /* Descending order */
}

int maxSumKElements(int arr[], int n, int k) {
    /* Sort in descending order */
    qsort(arr, n, sizeof(int), compare);
    
    /* Sum first k elements */
    int sum = 0;
    for (int i = 0; i < k; i++)
        sum += arr[i];
    
    return sum;
}

int main(void) {
    int arr[] = {2, 1, 5, 1, 3, 2};
    int n = sizeof(arr) / sizeof(arr[0]);
    int k = 3;
    
    printf("Array: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\nk = %d\n", k);
    
    int result = maxSumKElements(arr, n, k);
    printf("Maximum sum of %d elements: %d\n", k, result);
    
    return 0;
}
