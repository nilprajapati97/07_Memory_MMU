/*
================================================================================
                    METHOD 1: SIMPLE SHIFTING APPROACH
================================================================================

EXPLANATION:
-----------
This is the simplest method to remove duplicates from an array.
It works by comparing each element with all elements after it.
If a duplicate is found, we shift all elements one position to the left,
effectively removing the duplicate.

HOW IT WORKS:
1. Start with first element
2. Compare it with all elements after it
3. If match found (duplicate detected):
   - Shift all elements from that position to the left by 1
   - Reduce array size by 1
   - Check the same position again (j--)
4. Move to next element
5. Repeat until all elements are checked

TIME COMPLEXITY: O(n²)
  - Outer loop: n iterations
  - Inner loop: up to n iterations
  - Shifting: up to n operations
  - Worst case: n² operations

SPACE COMPLEXITY: O(1)
  - No extra space needed (modifies array in-place)
  - Only uses a few variables (i, j, k, newSize)

WHEN TO USE:
  ✓ Learning/Understanding duplicates
  ✓ Very small arrays (< 10 elements)
  ✓ When memory is extremely limited
  ✓ No specific order requirements

WHEN NOT TO USE:
  ✗ Large arrays (slow due to O(n²))
  ✗ Performance-critical applications
  ✗ Need to preserve original order

ADVANTAGES:
  + Very simple to understand
  + No extra memory needed
  + Good for learning purposes
  + Works with any unsorted array

DISADVANTAGES:
  - Slowest method (O(n²))
  - Multiple array shifts needed
  - Not practical for large data

EXAMPLE TRACE:
  Original:   [1, 2, 2, 3, 4, 4, 5, 1]
  
  i=0, arr[0]=1:
    j=1: arr[1]=2 (no match)
    j=2: arr[2]=2 (no match) 
    j=3: arr[3]=3 (no match)
    j=4: arr[4]=4 (no match)
    j=5: arr[5]=4 (no match)
    j=6: arr[6]=5 (no match)
    j=7: arr[7]=1 (MATCH found!)
         Shift: [1, 2, 2, 3, 4, 4, 5]
         newSize = 7, j becomes 6
    j=6: arr[6]=5 (no match)
    
  Result:     [1, 2, 3, 4, 5] with duplicates removed

================================================================================
*/

#include <stdio.h>

/*
Function: removeDuplicates_Simple
Purpose: Remove duplicate elements by shifting array elements left
Parameters:
  - arr: pointer to integer array
  - n: number of elements in array
Returns:
  - New size of array after removing duplicates
*/
int removeDuplicates_Simple(int arr[], int n) {
    int i, j, k;
    int newSize = n;  // Track new size of array
    
    /*
    Outer loop: goes through each element
    For each element, check all elements after it for duplicates
    */
    for (i = 0; i < newSize; i++) {
        
        /*
        Inner loop: compare current element with all following elements
        */
        for (j = i + 1; j < newSize; j++) {
            
            /*
            If we find a duplicate element
            */
            if (arr[i] == arr[j]) {
                /*
                Shift all elements from position j to the left by 1
                This removes the duplicate at position j
                
                Example: [1, 2, 2, 3] at j=2 (where duplicate is)
                Loop k from 2 to 2:
                  arr[2] = arr[3] → [1, 2, 3, 3]
                Result: [1, 2, 3] with size decreased
                */
                for (k = j; k < newSize - 1; k++) {
                    arr[k] = arr[k + 1];
                }
                
                /*
                Reduce size since we removed one element
                */
                newSize--;
                
                /*
                Decrease j so we check the same position again
                (next element has shifted to this position)
                */
                j--;
            }
        }
    }
    
    return newSize;
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
Purpose: Test the simple shifting method with examples
*/
int main() {
    printf("================================================================================\n");
    printf("               METHOD 1: SIMPLE SHIFTING APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Array with multiple duplicates
    printf("TEST CASE 1: Array with multiple duplicates\n");
    printf("---------------------------------------------\n");
    int arr1[] = {1, 2, 2, 3, 4, 4, 5, 1};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    
    printf("Original array:        ");
    printArray(arr1, n1);
    printf("Size before:           %d\n", n1);
    
    int newSize1 = removeDuplicates_Simple(arr1, n1);
    
    printf("After removing duplicates: ");
    printArray(arr1, newSize1);
    printf("Size after:            %d\n", newSize1);
    printf("Duplicates removed:    %d\n\n", n1 - newSize1);
    
    // TEST CASE 2: Array with consecutive duplicates
    printf("TEST CASE 2: Array with consecutive duplicates\n");
    printf("-----------------------------------------------\n");
    int arr2[] = {5, 5, 5, 3, 3, 1, 1, 1};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    
    printf("Original array:        ");
    printArray(arr2, n2);
    printf("Size before:           %d\n", n2);
    
    int newSize2 = removeDuplicates_Simple(arr2, n2);
    
    printf("After removing duplicates: ");
    printArray(arr2, newSize2);
    printf("Size after:            %d\n", newSize2);
    printf("Duplicates removed:    %d\n\n", n2 - newSize2);
    
    // TEST CASE 3: Array with no duplicates
    printf("TEST CASE 3: Array with NO duplicates\n");
    printf("-------------------------------------\n");
    int arr3[] = {1, 2, 3, 4, 5};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    
    printf("Original array:        ");
    printArray(arr3, n3);
    printf("Size before:           %d\n", n3);
    
    int newSize3 = removeDuplicates_Simple(arr3, n3);
    
    printf("After removing duplicates: ");
    printArray(arr3, newSize3);
    printf("Size after:            %d\n", newSize3);
    printf("Duplicates removed:    0\n\n");
    
    // TEST CASE 4: All duplicates
    printf("TEST CASE 4: All elements are duplicates\n");
    printf("----------------------------------------\n");
    int arr4[] = {7, 7, 7, 7, 7};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    
    printf("Original array:        ");
    printArray(arr4, n4);
    printf("Size before:           %d\n", n4);
    
    int newSize4 = removeDuplicates_Simple(arr4, n4);
    
    printf("After removing duplicates: ");
    printArray(arr4, newSize4);
    printf("Size after:            %d\n", newSize4);
    printf("Duplicates removed:    %d\n\n", n4 - newSize4);
    
    // TEST CASE 5: Single element
    printf("TEST CASE 5: Single element\n");
    printf("---------------------------\n");
    int arr5[] = {42};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    
    printf("Original array:        ");
    printArray(arr5, n5);
    printf("Size before:           %d\n", n5);
    
    int newSize5 = removeDuplicates_Simple(arr5, n5);
    
    printf("After removing duplicates: ");
    printArray(arr5, newSize5);
    printf("Size after:            %d\n", newSize5);
    printf("Duplicates removed:    0\n\n");
    
    // TEST CASE 6: Mixed unsorted
    printf("TEST CASE 6: Mixed unsorted array\n");
    printf("---------------------------------\n");
    int arr6[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    
    printf("Original array:        ");
    printArray(arr6, n6);
    printf("Size before:           %d\n", n6);
    
    int newSize6 = removeDuplicates_Simple(arr6, n6);
    
    printf("After removing duplicates: ");
    printArray(arr6, newSize6);
    printf("Size after:            %d\n", newSize6);
    printf("Duplicates removed:    %d\n\n", n6 - newSize6);
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n²) - Worst case for unsorted array\n");
    printf("  Space Complexity: O(1)  - In-place modification, no extra space\n");
    printf("  Pros:  Simple to understand and implement\n");
    printf("  Cons:  Very slow for large arrays\n");
    printf("================================================================================\n");
    
    return 0;
}
