/*
================================================================================
              METHOD 6: ORDER-PRESERVING APPROACH
================================================================================

EXPLANATION:
-----------
This method removes duplicates while maintaining the original order of elements.
It uses a flag array to track which elements have already been seen.
Unlike Method 2, it preserves the original sequence as the input array appears.

WHY PRESERVE ORDER?
When processing data sequentially (logs, events, records), the order of
first appearance often matters. This method keeps that important information.

HOW IT WORKS:
1. Create a flag array to track which elements we've already encountered
2. For each element in original array:
   a. Check if we've seen this element before (look at flag array)
   b. If NOT seen before (first appearance):
      - Mark it as seen in flag array
      - Add to result array
      - Increment result counter
   c. If already seen (duplicate):
      - Skip this element
3. Return the result array with unique elements in original order

KEY INSIGHT:
We only add an element when we encounter it for the FIRST time.
Subsequent appearances are skipped.
This maintains original sequence.

FLOW DIAGRAM:
Original: [3, 1, 3, 2, 1, 4, 2]

Process 3: First time seeing 3 → Add to result [3], mark visited
Process 1: First time seeing 1 → Add to result [3, 1], mark visited  
Process 3: Already seen 3 → Skip (duplicate)
Process 2: First time seeing 2 → Add to result [3, 1, 2], mark visited
Process 1: Already seen 1 → Skip (duplicate)
Process 4: First time seeing 4 → Add to result [3, 1, 2, 4], mark visited
Process 2: Already seen 2 → Skip (duplicate)

Result: [3, 1, 2, 4] (unique, original order preserved)

TIME COMPLEXITY: O(n²)
  - Outer loop: n iterations
  - Inner loop: up to n iterations (checking if seen before)
  - Total: O(n²)
  
  Alternative simpler: O(n) if we use hash table instead of array

SPACE COMPLEXITY: O(n)
  - Result array: up to n elements
  - Flag array: exactly n elements
  - Total: O(n)

WHEN TO USE:
  ✓ Order of appearance is important
  ✓ Unsorted array
  ✓ Need to preserve first occurrence position
  ✓ Processing logs or time-series data
  ✓ Event sequences where order matters

WHEN NOT TO USE:
  ✗ Memory is very limited
  ✗ Array is very large and speed critical
  ✗ Sorted result is acceptable
  ✗ Range is small (use hash method instead)

ADVANTAGES:
  + Preserves original order
  + Works with unsorted arrays
  + Easy to understand logic
  + Maintains first-appearance sequence
  + Flexible for various data types

DISADVANTAGES:
  - O(n²) time complexity (check all previous)
  - Uses O(n) extra space
  - Not optimal for large arrays
  - Slower than sort-based methods

REAL-WORLD EXAMPLES WHERE ORDER MATTERS:
1. Website visitor logs: Keep first visit order
2. Access logs: Track first access per user
3. Event sequences: Maintain chronological order
4. Transaction history: Preserve sequence
5. Unique values in streaming data

EXAMPLE TRACE:
  Original: [3, 1, 3, 2, 1, 4, 2]
  
  i=0, arr[0]=3:
    Check visited[0:0], not marked as seen
    → Add to result: [3]
    → Mark visited[0] = 1
    → count = 1
  
  i=1, arr[1]=1:
    Check visited[0:1]
      visited[0]=1? No (different element)
      visited[1] doesn't exist yet
    → Not seen before
    → Add to result: [3, 1]
    → Mark visited[1] = 1
    → count = 2
  
  i=2, arr[2]=3:
    Check visited[0:2]
      visited[0]=1 for arr[0]=3
      Current arr[2]=3, already in result[0]
    → Already seen (through visited flag)
    → Skip this element
  
  i=3, arr[3]=2:
    Check visited[0:3]
      visited[0]=1 for arr[0]=3
      visited[1]=1 for arr[1]=1
      Current arr[3]=2, not matched yet
    → Not seen before
    → Add to result: [3, 1, 2]
    → Mark visited[3] = 1
    → count = 3
  
  i=4, arr[4]=1:
    Check visited[0:4]
      visited[1]=1 for arr[1]=1
      Current arr[4]=1
    → Already seen
    → Skip
  
  i=5, arr[5]=4:
    Check visited[0:5], not seen
    → Add to result: [3, 1, 2, 4]
    → count = 4
  
  i=6, arr[6]=2:
    Check visited[0:6]
      visited[3]=1 for arr[3]=2
      Current arr[6]=2
    → Already seen
    → Skip
  
  Result: [3, 1, 2, 4] with count=4 (order preserved!)

================================================================================
*/

#include <stdio.h>
#include <stdlib.h>

/*
Function: removeDuplicates_OrderPreserve
Purpose: Remove duplicates while preserving original order
Parameters:
  - arr: pointer to original integer array
  - n: number of elements in array
  - result: pointer to result array (where unique elements are stored)
Returns:
  - Count of unique elements
*/
int removeDuplicates_OrderPreserve(int arr[], int n, int result[]) {
    /*
    Create flag array to track which indices we've already processed
    Each index represents an element from original array
    visited[i] = 1 means arr[i] has been added to result (first occurrence)
    visited[i] = 0 means arr[i] either not processed or is a duplicate
    */
    int *visited = (int *)calloc(n, sizeof(int));
    int resultCount = 0;  // Counter for unique elements
    
    /*
    For each element in original array
    */
    for (int i = 0; i < n; i++) {
        int isDuplicate = 0;  // Assume unique initially
        
        /*
        Check all previous elements (indices 0 to i-1)
        to see if we've already encountered this value
        */
        for (int j = 0; j < i; j++) {
            /*
            If we find same value in an earlier index
            AND that earlier index was marked as visited
            (meaning it was added to result)
            Then current element is a duplicate
            */
            if (arr[i] == arr[j] && visited[j]) {
                isDuplicate = 1;  // Mark as duplicate
                break;  // No need to check further
            }
        }
        
        /*
        If element is NOT a duplicate (first occurrence of this value)
        */
        if (!isDuplicate) {
            result[resultCount] = arr[i];  // Add to result
            resultCount++;  // Increment count
            visited[i] = 1;  // Mark this index as processed
        }
    }
    
    /*
    Free the flag array memory
    */
    free(visited);
    
    return resultCount;
}

/*
Function: printArray
Purpose: Display array elements
*/
void printArray(int arr[], int n) {
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

/*
Function: main
Purpose: Test the order-preserving method
*/
int main() {
    printf("================================================================================\n");
    printf("          METHOD 6: ORDER-PRESERVING APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Basic unsorted array
    printf("TEST CASE 1: Basic unsorted array with duplicates\n");
    printf("------------------------------------------------\n");
    int arr1[] = {3, 1, 3, 2, 1, 4, 2};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    int result1[100];
    
    printf("Original array:        ");
    printArray(arr1, n1);
    printf("Size before:           %d\n", n1);
    
    int newSize1 = removeDuplicates_OrderPreserve(arr1, n1, result1);
    
    printf("After removing duplicates: ");
    printArray(result1, newSize1);
    printf("Size after:            %d\n", newSize1);
    printf("Order preserved:       Yes (3, 1, 2, 4 as first appearance)\n");
    printf("Original unchanged:    ");
    printArray(arr1, n1);
    printf("\n");
    
    // TEST CASE 2: Random order with multiple duplicates
    printf("TEST CASE 2: Random order with multiple duplicates\n");
    printf("-------------------------------------------------\n");
    int arr2[] = {5, 2, 8, 2, 5, 1, 8, 3};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    int result2[100];
    
    printf("Original array:        ");
    printArray(arr2, n2);
    printf("Size before:           %d\n", n2);
    
    int newSize2 = removeDuplicates_OrderPreserve(arr2, n2, result2);
    
    printf("After removing duplicates: ");
    printArray(result2, newSize2);
    printf("Size after:            %d\n", newSize2);
    printf("Order preserved:       Yes (5, 2, 8, 1, 3 as first appearance)\n");
    printf("Duplicates removed:    %d\n\n", n2 - newSize2);
    
    // TEST CASE 3: All same elements
    printf("TEST CASE 3: All elements are the same\n");
    printf("-----------------------------------\n");
    int arr3[] = {7, 7, 7, 7, 7};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    int result3[100];
    
    printf("Original array:        ");
    printArray(arr3, n3);
    printf("Size before:           %d\n", n3);
    
    int newSize3 = removeDuplicates_OrderPreserve(arr3, n3, result3);
    
    printf("After removing duplicates: ");
    printArray(result3, newSize3);
    printf("Size after:            %d\n", newSize3);
    printf("Result:                Only first 7 kept\n\n");
    
    // TEST CASE 4: No duplicates
    printf("TEST CASE 4: No duplicates\n");
    printf("-------------------------\n");
    int arr4[] = {5, 2, 8, 1, 9};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    int result4[100];
    
    printf("Original array:        ");
    printArray(arr4, n4);
    printf("Size before:           %d\n", n4);
    
    int newSize4 = removeDuplicates_OrderPreserve(arr4, n4, result4);
    
    printf("After removing duplicates: ");
    printArray(result4, newSize4);
    printf("Size after:            %d\n", newSize4);
    printf("No changes:            Array already unique\n\n");
    
    // TEST CASE 5: Single element
    printf("TEST CASE 5: Single element\n");
    printf("--------------------------\n");
    int arr5[] = {42};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    int result5[100];
    
    printf("Original array:        ");
    printArray(arr5, n5);
    printf("Size before:           %d\n", n5);
    
    int newSize5 = removeDuplicates_OrderPreserve(arr5, n5, result5);
    
    printf("After removing duplicates: ");
    printArray(result5, newSize5);
    printf("Size after:            %d\n\n", newSize5);
    
    // TEST CASE 6: Two elements same
    printf("TEST CASE 6: Two elements (both same)\n");
    printf("-----------------------------------\n");
    int arr6[] = {10, 10};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    int result6[100];
    
    printf("Original array:        ");
    printArray(arr6, n6);
    printf("Size before:           %d\n", n6);
    
    int newSize6 = removeDuplicates_OrderPreserve(arr6, n6, result6);
    
    printf("After removing duplicates: ");
    printArray(result6, newSize6);
    printf("Size after:            %d\n\n", newSize6);
    
    // TEST CASE 7: Complex pattern
    printf("TEST CASE 7: Complex pattern with repeats\n");
    printf("---------------------------------------\n");
    int arr7[] = {1, 2, 1, 3, 2, 4, 3, 5, 4, 1};
    int n7 = sizeof(arr7) / sizeof(arr7[0]);
    int result7[100];
    
    printf("Original array:        ");
    printArray(arr7, n7);
    printf("Size before:           %d\n", n7);
    
    int newSize7 = removeDuplicates_OrderPreserve(arr7, n7, result7);
    
    printf("After removing duplicates: ");
    printArray(result7, newSize7);
    printf("Size after:            %d\n", newSize7);
    printf("First appearances:     1, 2, 3, 4, 5 (in that order)\n", newSize7);
    printf("Duplicates removed:    %d\n\n", n7 - newSize7);
    
    // TEST CASE 8: Negative numbers
    printf("TEST CASE 8: Array with negative numbers\n");
    printf("--------------------------------------\n");
    int arr8[] = {-5, 3, -5, 0, 3, -2};
    int n8 = sizeof(arr8) / sizeof(arr8[0]);
    int result8[100];
    
    printf("Original array:        ");
    printArray(arr8, n8);
    printf("Size before:           %d\n", n8);
    
    int newSize8 = removeDuplicates_OrderPreserve(arr8, n8, result8);
    
    printf("After removing duplicates: ");
    printArray(result8, newSize8);
    printf("Size after:            %d\n", newSize8);
    printf("Order preserved:       Yes (-5, 3, 0, -2)\n\n");
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n²) - Nested loops to check previous elements\n");
    printf("  Space Complexity: O(n)  - Result array + flag array\n");
    printf("  Pros:  Preserves original order, works with unsorted arrays\n");
    printf("  Cons:  O(n²) time, uses extra space\n");
    printf("  Best Use: When order of first appearance matters\n");
    printf("================================================================================\n");
    
    return 0;
}
