/*
================================================================================
                   METHOD 3: EXTRA ARRAY APPROACH
================================================================================

EXPLANATION:
-----------
This method uses an additional array to store unique elements.
It preserves the original order of elements and works with unsorted arrays.
For each element in original array, we check if it's already in the result array.
If not, we add it to the result array.

HOW IT WORKS:
1. Create an empty result array to store unique elements
2. Initialize a counter for result array position
3. For each element in original array:
   a. Check if element already exists in result array
   b. If NOT found (not duplicate):
      - Add element to result array
      - Increment result array counter
   c. If found (is duplicate):
      - Skip this element
4. Return the count of unique elements

FLOW DIAGRAM:
Original: [5, 3, 5, 1, 3, 2]
           ↓
Process 5: Not in result → Add to result → [5]
           ↓
Process 3: Not in result → Add to result → [5, 3]
           ↓
Process 5: Already in result → Skip
           ↓
Process 1: Not in result → Add to result → [5, 3, 1]
           ↓
Process 3: Already in result → Skip
           ↓
Process 2: Not in result → Add to result → [5, 3, 1, 2]

Result: [5, 3, 1, 2] (unique, order preserved)

TIME COMPLEXITY: O(n²)
  - Outer loop: n iterations (for each element)
  - Inner loop: up to n iterations (checking in result array)
  - Total: n × n = O(n²)

SPACE COMPLEXITY: O(n)
  - Extra array of size up to n
  - Stores all unique elements

WHEN TO USE:
  ✓ Array is unsorted
  ✓ Need to preserve original order
  ✓ Size of array is small to medium
  ✓ Learning purpose
  ✓ When you need output in separate array

WHEN NOT TO USE:
  ✗ Memory is very limited
  ✗ Array is very large (O(n²) too slow)
  ✗ Sorted data available (use other methods)
  ✗ Performance critical application

ADVANTAGES:
  + Works with unsorted arrays
  + Preserves original order
  + Doesn't modify original array
  + Easy to understand
  + Result is separate (original unchanged)

DISADVANTAGES:
  - Uses O(n) extra space
  - O(n²) time complexity
  - Not optimal for large arrays
  - Still slow due to nested loop

EXAMPLE TRACE:
  Original: [5, 3, 5, 1, 3, 2]
  
  i=0: Check 5 in result[]
       Not found → Add 5
       result = [5], count = 1
       
  i=1: Check 3 in result[]
       Not found → Add 3
       result = [5, 3], count = 2
       
  i=2: Check 5 in result[]
       Found at j=0 → Skip (isDuplicate=1)
       
  i=3: Check 1 in result[]
       Not found → Add 1
       result = [5, 3, 1], count = 3
       
  i=4: Check 3 in result[]
       Found at j=1 → Skip (isDuplicate=1)
       
  i=5: Check 2 in result[]
       Not found → Add 2
       result = [5, 3, 1, 2], count = 4
  
  Final result: [5, 3, 1, 2] with count = 4

================================================================================
*/

#include <stdio.h>
#include <stdlib.h>

/*
Function: removeDuplicates_Extra
Purpose: Remove duplicates using an extra array to store unique elements
Parameters:
  - arr: pointer to original integer array (unsorted or sorted)
  - n: number of elements in original array
  - result: pointer to result array where unique elements will be stored
Returns:
  - Count of unique elements
*/
int removeDuplicates_Extra(int arr[], int n, int result[]) {
    int count = 0;  // Counter for unique elements in result array
    int isDuplicate;  // Flag to check if element is duplicate
    
    /*
    For each element in original array
    */
    for (int i = 0; i < n; i++) {
        isDuplicate = 0;  // Assume element is unique initially
        
        /*
        Check if this element already exists in result array
        */
        for (int j = 0; j < count; j++) {
            /*
            If element found in result array, it's a duplicate
            */
            if (arr[i] == result[j]) {
                isDuplicate = 1;  // Mark as duplicate
                break;  // No need to check further
            }
        }
        
        /*
        If element is not a duplicate, add it to result array
        */
        if (!isDuplicate) {
            result[count] = arr[i];  // Add element to result
            count++;  // Increment count of unique elements
        }
    }
    
    return count;
}

/*
Function: printArray
Purpose: Display array elements with formatting
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
Purpose: Test the extra array method with examples
*/
int main() {
    printf("================================================================================\n");
    printf("              METHOD 3: EXTRA ARRAY APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Unsorted array with duplicates
    printf("TEST CASE 1: Unsorted array with duplicates\n");
    printf("------------------------------------------\n");
    int arr1[] = {5, 3, 5, 1, 3, 2, 1, 4};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    int result1[100];  // Extra array to store results
    
    printf("Original array:        ");
    printArray(arr1, n1);
    printf("Size:                  %d\n", n1);
    
    int newSize1 = removeDuplicates_Extra(arr1, n1, result1);
    
    printf("After removing duplicates: ");
    printArray(result1, newSize1);
    printf("Size:                  %d\n", newSize1);
    printf("Unique elements:       %d\n", newSize1);
    printf("Duplicates removed:    %d\n", n1 - newSize1);
    printf("Original unchanged:    ");
    printArray(arr1, n1);
    printf("\n");
    
    // TEST CASE 2: Array with consecutive duplicates
    printf("TEST CASE 2: Array with consecutive duplicates\n");
    printf("---------------------------------------------\n");
    int arr2[] = {1, 1, 2, 2, 3, 3};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    int result2[100];
    
    printf("Original array:        ");
    printArray(arr2, n2);
    printf("Size:                  %d\n", n2);
    
    int newSize2 = removeDuplicates_Extra(arr2, n2, result2);
    
    printf("After removing duplicates: ");
    printArray(result2, newSize2);
    printf("Size:                  %d\n", newSize2);
    printf("Unique elements:       %d\n", newSize2);
    printf("Duplicates removed:    %d\n", n2 - newSize2);
    printf("Order preserved:       Yes (as in original)\n\n");
    
    // TEST CASE 3: All elements same
    printf("TEST CASE 3: All elements are the same\n");
    printf("-------------------------------------\n");
    int arr3[] = {7, 7, 7, 7, 7};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    int result3[100];
    
    printf("Original array:        ");
    printArray(arr3, n3);
    printf("Size:                  %d\n", n3);
    
    int newSize3 = removeDuplicates_Extra(arr3, n3, result3);
    
    printf("After removing duplicates: ");
    printArray(result3, newSize3);
    printf("Size:                  %d\n", newSize3);
    printf("Unique elements:       %d\n", newSize3);
    printf("Duplicates removed:    %d\n\n", n3 - newSize3);
    
    // TEST CASE 4: No duplicates
    printf("TEST CASE 4: No duplicates\n");
    printf("--------------------------\n");
    int arr4[] = {10, 20, 30, 40, 50};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    int result4[100];
    
    printf("Original array:        ");
    printArray(arr4, n4);
    printf("Size:                  %d\n", n4);
    
    int newSize4 = removeDuplicates_Extra(arr4, n4, result4);
    
    printf("After removing duplicates: ");
    printArray(result4, newSize4);
    printf("Size:                  %d\n", newSize4);
    printf("Unique elements:       %d\n", newSize4);
    printf("Duplicates removed:    0\n\n");
    
    // TEST CASE 5: Single element
    printf("TEST CASE 5: Single element\n");
    printf("---------------------------\n");
    int arr5[] = {99};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    int result5[100];
    
    printf("Original array:        ");
    printArray(arr5, n5);
    printf("Size:                  %d\n", n5);
    
    int newSize5 = removeDuplicates_Extra(arr5, n5, result5);
    
    printf("After removing duplicates: ");
    printArray(result5, newSize5);
    printf("Size:                  %d\n", newSize5);
    printf("Unique elements:       %d\n\n", newSize5);
    
    // TEST CASE 6: Negative and zero
    printf("TEST CASE 6: Array with negative numbers and zero\n");
    printf("-------------------------------------------------\n");
    int arr6[] = {-5, 0, -5, 10, 0, 10, -2};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    int result6[100];
    
    printf("Original array:        ");
    printArray(arr6, n6);
    printf("Size:                  %d\n", n6);
    
    int newSize6 = removeDuplicates_Extra(arr6, n6, result6);
    
    printf("After removing duplicates: ");
    printArray(result6, newSize6);
    printf("Size:                  %d\n", newSize6);
    printf("Unique elements:       %d\n", newSize6);
    printf("Duplicates removed:    %d\n\n", n6 - newSize6);
    
    // TEST CASE 7: Two elements, both same
    printf("TEST CASE 7: Two elements, both same\n");
    printf("-----------------------------------\n");
    int arr7[] = {42, 42};
    int n7 = sizeof(arr7) / sizeof(arr7[0]);
    int result7[100];
    
    printf("Original array:        ");
    printArray(arr7, n7);
    printf("Size:                  %d\n", n7);
    
    int newSize7 = removeDuplicates_Extra(arr7, n7, result7);
    
    printf("After removing duplicates: ");
    printArray(result7, newSize7);
    printf("Size:                  %d\n", newSize7);
    printf("Unique elements:       %d\n", newSize7);
    printf("Duplicates removed:    %d\n\n", n7 - newSize7);
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n²) - Nested loop checking\n");
    printf("  Space Complexity: O(n)  - Extra array to store results\n");
    printf("  Pros:  Works with unsorted arrays, preserves order, simple logic\n");
    printf("  Cons:  Uses extra space, O(n²) makes it slow for large arrays\n");
    printf("  Best Use: Unsorted arrays where order preservation is important\n");
    printf("================================================================================\n");
    
    return 0;
}
