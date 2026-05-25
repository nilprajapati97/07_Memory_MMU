/*
================================================================================
                   METHOD 2: SORTED ARRAY APPROACH
================================================================================

EXPLANATION:
-----------
This method is used when the array is ALREADY SORTED or after sorting it.
It works by comparing adjacent elements and moving unique elements forward.
This is a very efficient method for sorted arrays.

HOW IT WORKS:
1. Keep track of position where we write unique elements (writeIndex)
2. Start from second element
3. Compare current element with element at writeIndex
4. If they are different (unique), increment writeIndex and copy element
5. If they are same (duplicate), skip this element
6. Continue until all elements are processed
7. Return writeIndex + 1 as the new array size

KEY INSIGHT:
The array remains partially filled with unique elements at the beginning.
We don't need to shift elements; we just overwrite duplicates!

TIME COMPLEXITY: O(n)
  - Single pass through array
  - No nested loops
  - Linear time = n operations

SPACE COMPLEXITY: O(1)
  - No extra space needed
  - Only uses writeIndex variable
  - In-place modification

WHEN TO USE:
  ✓ Array is already sorted
  ✓ Speed is important (need O(n))
  ✓ Memory is limited
  ✓ Can sort first then use this method
  ✓ Large arrays where O(n log n) is acceptable

WHEN NOT TO USE:
  ✗ Array is not sorted and sorting is expensive
  ✗ Order must be preserved as original
  ✗ Need original array intact

ADVANTAGES:
  + Very fast O(n) for sorted arrays
  + No extra memory needed (in-place)
  + Simple and elegant solution
  + Optimal for sorted data

DISADVANTAGES:
  - Requires sorted array (O(n log n) if need to sort first)
  - Changes original order
  - Not suitable for unsorted data directly

EXAMPLE TRACE:
  Original (sorted):  [1, 1, 2, 2, 3, 3, 4, 5, 5]
  
  writeIndex = 0, arr[0] = 1
  
  i=1: arr[1]=1, arr[0]=1 (SAME - duplicate, skip)
  i=2: arr[2]=2, arr[0]=1 (DIFFERENT - unique!)
       writeIndex = 1
       arr[1] = arr[2] = 2
       Array now: [1, 2, 2, 3, 3, 4, 5, 5]
  
  i=3: arr[3]=2, arr[1]=2 (SAME - duplicate, skip)
  i=4: arr[4]=3, arr[1]=2 (DIFFERENT - unique!)
       writeIndex = 2
       arr[2] = arr[4] = 3
       Array now: [1, 2, 3, 3, 4, 5, 5]
  
  i=5: arr[5]=3, arr[2]=3 (SAME - duplicate, skip)
  i=6: arr[6]=4, arr[2]=3 (DIFFERENT - unique!)
       writeIndex = 3
       arr[3] = arr[6] = 4
       
  i=7: arr[7]=5, arr[3]=4 (DIFFERENT - unique!)
       writeIndex = 4
       arr[4] = arr[7] = 5
  
  i=8: arr[8]=5, arr[4]=5 (SAME - duplicate, skip)
  
  Result: [1, 2, 3, 4, 5] with writeIndex + 1 = 5

================================================================================
*/

#include <stdio.h>

/*
Function: removeDuplicates_Sorted
Purpose: Remove duplicates from an already sorted array
Parameters:
  - arr: pointer to sorted integer array
  - n: number of elements in array
Returns:
  - New size of array after removing duplicates
Note: Array must be sorted before calling this function
*/
int removeDuplicates_Sorted(int arr[], int n) {
    /*
    Edge case: if array has 0 or 1 element, no duplicates possible
    */
    if (n <= 1)
        return n;
    
    /*
    writeIndex: position where we place the next unique element
    Start at 0 because first element is always unique
    */
    int writeIndex = 0;
    
    /*
    Start from second element (index 1)
    Compare each element with element at writeIndex
    */
    for (int i = 1; i < n; i++) {
        /*
        If current element is different from element at writeIndex
        Then it's a new unique element
        */
        if (arr[i] != arr[writeIndex]) {
            /*
            Move writeIndex forward
            */
            writeIndex++;
            
            /*
            Copy the unique element to the new writeIndex position
            This overwrites any duplicate that was there
            */
            arr[writeIndex] = arr[i];
        }
        /*
        If arr[i] == arr[writeIndex], it's a duplicate
        We simply skip it (don't increment writeIndex)
        */
    }
    
    /*
    Return the count of unique elements
    Since writeIndex points to last unique element,
    total count is writeIndex + 1
    */
    return writeIndex + 1;
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
Purpose: Test the sorted array method with examples
*/
int main() {
    printf("================================================================================\n");
    printf("              METHOD 2: SORTED ARRAY APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Sorted array with duplicates
    printf("TEST CASE 1: Sorted array with multiple duplicates\n");
    printf("--------------------------------------------------\n");
    int arr1[] = {1, 1, 2, 2, 3, 3, 4, 5, 5};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    
    printf("Original array (sorted): ");
    printArray(arr1, n1);
    printf("Size before:             %d\n", n1);
    
    int newSize1 = removeDuplicates_Sorted(arr1, n1);
    
    printf("After removing duplicates: ");
    printArray(arr1, newSize1);
    printf("Size after:              %d\n", newSize1);
    printf("Duplicates removed:      %d\n\n", n1 - newSize1);
    
    // TEST CASE 2: All same elements
    printf("TEST CASE 2: All elements are same\n");
    printf("----------------------------------\n");
    int arr2[] = {5, 5, 5, 5, 5, 5};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    
    printf("Original array (sorted): ");
    printArray(arr2, n2);
    printf("Size before:             %d\n", n2);
    
    int newSize2 = removeDuplicates_Sorted(arr2, n2);
    
    printf("After removing duplicates: ");
    printArray(arr2, newSize2);
    printf("Size after:              %d\n", newSize2);
    printf("Duplicates removed:      %d\n\n", n2 - newSize2);
    
    // TEST CASE 3: No duplicates
    printf("TEST CASE 3: No duplicates (already unique)\n");
    printf("------------------------------------------\n");
    int arr3[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    
    printf("Original array (sorted): ");
    printArray(arr3, n3);
    printf("Size before:             %d\n", n3);
    
    int newSize3 = removeDuplicates_Sorted(arr3, n3);
    
    printf("After removing duplicates: ");
    printArray(arr3, newSize3);
    printf("Size after:              %d\n", newSize3);
    printf("Duplicates removed:      0\n\n");
    
    // TEST CASE 4: Single element
    printf("TEST CASE 4: Single element\n");
    printf("---------------------------\n");
    int arr4[] = {10};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    
    printf("Original array (sorted): ");
    printArray(arr4, n4);
    printf("Size before:             %d\n", n4);
    
    int newSize4 = removeDuplicates_Sorted(arr4, n4);
    
    printf("After removing duplicates: ");
    printArray(arr4, newSize4);
    printf("Size after:              %d\n", newSize4);
    printf("Duplicates removed:      0\n\n");
    
    // TEST CASE 5: Two elements, both same
    printf("TEST CASE 5: Two elements, both same\n");
    printf("------------------------------------\n");
    int arr5[] = {7, 7};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    
    printf("Original array (sorted): ");
    printArray(arr5, n5);
    printf("Size before:             %d\n", n5);
    
    int newSize5 = removeDuplicates_Sorted(arr5, n5);
    
    printf("After removing duplicates: ");
    printArray(arr5, newSize5);
    printf("Size after:              %d\n", newSize5);
    printf("Duplicates removed:      %d\n\n", n5 - newSize5);
    
    // TEST CASE 6: Larger sorted array with mixed duplicates
    printf("TEST CASE 6: Larger sorted array\n");
    printf("--------------------------------\n");
    int arr6[] = {1, 1, 1, 2, 2, 3, 4, 4, 4, 4, 5, 6, 6, 7, 8, 8, 8, 9, 10, 10};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    
    printf("Original array (sorted): ");
    printArray(arr6, n6);
    printf("Size before:             %d\n", n6);
    
    int newSize6 = removeDuplicates_Sorted(arr6, n6);
    
    printf("After removing duplicates: ");
    printArray(arr6, newSize6);
    printf("Size after:              %d\n", newSize6);
    printf("Duplicates removed:      %d\n\n", n6 - newSize6);
    
    // TEST CASE 7: Negative numbers in sorted array
    printf("TEST CASE 7: Sorted array with negative numbers\n");
    printf("----------------------------------------------\n");
    int arr7[] = {-5, -5, -2, -2, -2, 0, 0, 3, 3, 5};
    int n7 = sizeof(arr7) / sizeof(arr7[0]);
    
    printf("Original array (sorted): ");
    printArray(arr7, n7);
    printf("Size before:             %d\n", n7);
    
    int newSize7 = removeDuplicates_Sorted(arr7, n7);
    
    printf("After removing duplicates: ");
    printArray(arr7, newSize7);
    printf("Size after:              %d\n", newSize7);
    printf("Duplicates removed:      %d\n\n", n7 - newSize7);
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n)  - Single pass through array\n");
    printf("  Space Complexity: O(1)  - In-place modification, no extra space\n");
    printf("  Pros:  Very fast, optimal for sorted arrays\n");
    printf("  Cons:  Requires array to be sorted first\n");
    printf("  Best Use: When array is already sorted or sorting cost is acceptable\n");
    printf("================================================================================\n");
    
    return 0;
}
