#include <stdio.h>

/*
 * Problem: Maximum Sum of Subarray of Size K (Sliding Window)
 * 
 * Given an array of integers and a number k, find the maximum sum
 * of any contiguous subarray of size k.
 * 
 * Input: arr = [2, 1, 5, 1, 3, 2], k = 3
 * Output: 9
 * Explanation: 
 *   Window [2, 1, 5] → sum = 8
 *   Window [1, 5, 1] → sum = 7
 *   Window [5, 1, 3] → sum = 9 ✓ (maximum)
 *   Window [1, 3, 2] → sum = 6
 * 
 * Approach: Sliding Window
 * Time: O(n)
 * Space: O(1)
 */

int maxSumSubarray(int arr[], int n, int k) {
    if (n < k)
        return -1;
    
    /* Calculate sum of first window */
    int windowSum = 0;
    for (int i = 0; i < k; i++)
        windowSum += arr[i];
    
    int maxSum = windowSum;
    
    /* Slide the window */
    for (int i = k; i < n; i++) {
        windowSum += arr[i] - arr[i - k];  /* Add new, remove old */
        if (windowSum > maxSum)
            maxSum = windowSum;
    }
    
    return maxSum;
}

int main(void) {
    int arr[] = {2, 1, 5, 1, 3, 2};
    int n = sizeof(arr) / sizeof(arr[0]);
    int k = 3;
    
    printf("Array: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\nk = %d\n\n", k);
    
    /* Show all windows */
    printf("All windows of size %d:\n", k);
    for (int i = 0; i <= n - k; i++) {
        printf("  [");
        int sum = 0;
        for (int j = i; j < i + k; j++) {
            printf("%d", arr[j]);
            if (j < i + k - 1) printf(", ");
            sum += arr[j];
        }
        printf("] → sum = %d\n", sum);
    }
    
    int result = maxSumSubarray(arr, n, k);
    printf("\nMaximum sum: %d\n", result);
    
    return 0;
}
