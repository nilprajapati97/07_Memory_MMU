#include <stdio.h>
#include <stdlib.h>

/*
 * Two interpretations of "Maximum Sum of K Elements":
 * 
 * 1. Maximum K Elements: Pick any k largest elements (non-contiguous)
 * 2. Sliding Window: Pick k contiguous elements with max sum
 * 
 * Input: arr = [2, 1, 5, 1, 3, 2], k = 3
 * 
 * Approach 1: Sort and sum largest k → 5+3+2 = 10
 * Approach 2: Sliding window → [5,1,3] = 9 ✓ (matches expected output)
 */

int compare(const void *a, const void *b) {
    return (*(int*)b - *(int*)a);
}

/* Approach 1: Any k largest elements */
int maxKElements(int arr[], int n, int k) {
    int *temp = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
        temp[i] = arr[i];
    
    qsort(temp, n, sizeof(int), compare);
    
    int sum = 0;
    for (int i = 0; i < k; i++)
        sum += temp[i];
    
    free(temp);
    return sum;
}

/* Approach 2: Contiguous k elements (Sliding Window) */
int maxSumSubarray(int arr[], int n, int k) {
    int windowSum = 0;
    for (int i = 0; i < k; i++)
        windowSum += arr[i];
    
    int maxSum = windowSum;
    for (int i = k; i < n; i++) {
        windowSum += arr[i] - arr[i - k];
        if (windowSum > maxSum)
            maxSum = windowSum;
    }
    
    return maxSum;
}

int main(void) {
    int n, k;
    
    printf("Enter array size: ");
    scanf("%d", &n);
    
    int *arr = (int*)malloc(n * sizeof(int));
    
    printf("Enter %d elements: ", n);
    for (int i = 0; i < n; i++)
        scanf("%d", &arr[i]);
    
    printf("Enter k: ");
    scanf("%d", &k);
    
    printf("\nArray: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n\n");
    
    int result1 = maxKElements(arr, n, k);
    int result2 = maxSumSubarray(arr, n, k);
    
    printf("Approach 1 (Any k largest): %d\n", result1);
    printf("Approach 2 (Contiguous k):  %d\n", result2);
    
    printf("\nFor arr=[2,1,5,1,3,2], k=3:\n");
    printf("  Expected output: 9\n");
    printf("  → Use Sliding Window approach\n");
    
    free(arr);
    return 0;
}
