/*
================================================================================
                  METHOD 5: SORT-FIRST APPROACH
================================================================================

EXPLANATION:
-----------
This method combines sorting with efficient duplicate removal.
First, we sort the unsorted array, then apply the efficient O(n) method
for sorted arrays to remove duplicates.
This provides a good balance of time and space complexity.

HOW IT WORKS:
1. Sort the array using Quick Sort or Merge Sort
2. Once sorted, use the efficient two-pointer method
3. Compare adjacent elements
4. Move unique elements forward using writeIndex

ALGORITHM STEPS:
  a) Partition and sort using Quick Sort
  b) After sorting: compare arr[i] with arr[writeIndex]
  c) If different (unique): increment writeIndex, copy element
  d) If same (duplicate): skip
  e) Continue until end of array

SORTING METHOD: Quick Sort (chosen for in-place sorting)
- Time: O(n log n) average, O(n²) worst
- Space: O(log n) recursive stack
- In-place: Yes (modifies array directly)

DUPLICATE REMOVAL: O(n) single pass after sorting

TOTAL TIME COMPLEXITY: O(n log n)
  - Sort: n log n
  - Remove duplicates: n
  - Total: n log n + n ≈ O(n log n)

SPACE COMPLEXITY: O(1)
  - Quick Sort uses O(log n) recursion stack
  - No extra array needed
  - In-place modification throughout

WHEN TO USE:
  ✓ Unsorted array (no sorting cost wasted)
  ✓ Need in-place modification (limited memory)
  ✓ Can sacrifice order (result is sorted)
  ✓ Good general-purpose solution
  ✓ Most practical for real-world scenarios
  ✓ Better than both methods 1 and 3 for large arrays

WHEN NOT TO USE:
  ✗ Array is already sorted (use method 2)
  ✗ Small integer range (use method 4 hash)
  ✗ Must preserve original order
  ✗ Stability matters (sorted data doesn't help)

ADVANTAGES:
  + Good balance of O(n log n) time
  + O(1) space (in-place)
  + Works with any unsorted array
  + Optimal for general-purpose use
  + Predictable performance
  + Better than O(n²) methods

DISADVANTAGES:
  - Changes element order (sorts data)
  - Slower than O(n) method 4 for small ranges
  - Slower than O(n) method 2 if data pre-sorted

EXAMPLE TRACE:
  Original: [7, 3, 7, 1, 9, 3, 2, 5, 1]
  
  STEP 1: Sort using Quick Sort
  Result:  [1, 1, 2, 3, 3, 5, 7, 7, 9]
  
  STEP 2: Remove duplicates from sorted array
  writeIndex = 0
  
  i=1: arr[1]=1, arr[0]=1 (same, skip)
  i=2: arr[2]=2, arr[0]=1 (different, unique!)
       writeIndex=1, arr[1]=2
       Array: [1, 2, 2, 3, 3, 5, 7, 7, 9]
  
  i=3: arr[3]=3, arr[1]=2 (different, unique!)
       writeIndex=2, arr[2]=3
       Array: [1, 2, 3, 3, 3, 5, 7, 7, 9]
  
  i=4: arr[4]=3, arr[2]=3 (same, skip)
  i=5: arr[5]=5, arr[2]=3 (different, unique!)
       writeIndex=3, arr[3]=5
  
  i=6: arr[6]=7, arr[3]=5 (different, unique!)
       writeIndex=4, arr[4]=7
  
  i=7: arr[7]=7, arr[4]=7 (same, skip)
  i=8: arr[8]=9, arr[4]=7 (different, unique!)
       writeIndex=5, arr[5]=9
  
  Result: [1, 2, 3, 5, 7, 9] (writeIndex + 1 = 6)

COMPARISON WITH OTHER METHODS:
  Method 1 (Simple): O(n²) - 100 × slower for 1000 elements
  Method 2 (Sorted): O(n) - But needs pre-sorted data
  Method 3 (Extra):  O(n²) - 100 × slower for 1000 elements
  Method 4 (Hash):   O(n) - But needs small range
  Method 5 (Sort):   O(n log n) - Good balance ✓

DECISION: Method 5 is best for most real-world scenarios!

================================================================================
*/

#include <stdio.h>

/*
Function: partition
Purpose: Partition array for Quick Sort
Parameters:
  - arr: pointer to array
  - low: starting index
  - high: ending index (pivot)
Returns:
  - Index of pivot in final position
*/
int partition(int arr[], int low, int high) {
    /*
    Choose last element as pivot
    */
    int pivot = arr[high];
    
    /*
    i will point to the position of pivot in sorted array
    Start from low - 1
    */
    int i = low - 1;
    
    /*
    Rearrange: smaller elements to left, larger to right
    */
    for (int j = low; j < high; j++) {
        /*
        If current element is smaller than pivot
        */
        if (arr[j] < pivot) {
            i++;
            /*
            Swap arr[i] and arr[j]
            */
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    
    /*
    Place pivot in its correct position
    */
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    
    return i + 1;
}

/*
Function: quickSort
Purpose: Sort array using Quick Sort algorithm
Parameters:
  - arr: pointer to array
  - low: starting index
  - high: ending index
*/
void quickSort(int arr[], int low, int high) {
    if (low < high) {
        /*
        Partition and get pivot index
        */
        int pi = partition(arr, low, high);
        
        /*
        Recursively sort left side
        */
        quickSort(arr, low, pi - 1);
        
        /*
        Recursively sort right side
        */
        quickSort(arr, pi + 1, high);
    }
}

/*
Function: removeDuplicates_SortFirst
Purpose: Remove duplicates by sorting then removing
Parameters:
  - arr: pointer to array
  - n: number of elements
Returns:
  - New size after removing duplicates
*/
int removeDuplicates_SortFirst(int arr[], int n) {
    if (n <= 1)
        return n;
    
    /*
    STEP 1: Sort the array
    Time: O(n log n)
    Space: O(log n) for recursion stack
    */
    quickSort(arr, 0, n - 1);
    
    /*
    STEP 2: Remove duplicates from sorted array
    Time: O(n)
    Space: O(1)
    */
    int writeIndex = 0;
    
    /*
    Compare each element with element at writeIndex
    */
    for (int i = 1; i < n; i++) {
        if (arr[i] != arr[writeIndex]) {
            writeIndex++;
            arr[writeIndex] = arr[i];
        }
    }
    
    return writeIndex + 1;
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
Purpose: Test the sort-first method
*/
int main() {
    printf("================================================================================\n");
    printf("              METHOD 5: SORT-FIRST APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Random unsorted array
    printf("TEST CASE 1: Random unsorted array\n");
    printf("---------------------------------\n");
    int arr1[] = {7, 3, 7, 1, 9, 3, 2, 5, 1};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    
    printf("Original array:        ");
    printArray(arr1, n1);
    printf("Size before:           %d\n", n1);
    
    int newSize1 = removeDuplicates_SortFirst(arr1, n1);
    
    printf("After sorting & removing: ");
    printArray(arr1, newSize1);
    printf("Size after:            %d\n", newSize1);
    printf("Result is SORTED:      Yes\n");
    printf("Duplicates removed:    %d\n\n", n1 - newSize1);
    
    // TEST CASE 2: Reverse sorted array
    printf("TEST CASE 2: Reverse sorted array\n");
    printf("--------------------------------\n");
    int arr2[] = {9, 8, 7, 7, 6, 5, 5, 4, 3, 3, 2, 1, 1};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    
    printf("Original array:        ");
    printArray(arr2, n2);
    printf("Size before:           %d\n", n2);
    
    int newSize2 = removeDuplicates_SortFirst(arr2, n2);
    
    printf("After sorting & removing: ");
    printArray(arr2, newSize2);
    printf("Size after:            %d\n", newSize2);
    printf("Duplicates removed:    %d\n\n", n2 - newSize2);
    
    // TEST CASE 3: All duplicates
    printf("TEST CASE 3: All elements are duplicates\n");
    printf("--------------------------------------\n");
    int arr3[] = {5, 5, 5, 5, 5, 5};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    
    printf("Original array:        ");
    printArray(arr3, n3);
    printf("Size before:           %d\n", n3);
    
    int newSize3 = removeDuplicates_SortFirst(arr3, n3);
    
    printf("After sorting & removing: ");
    printArray(arr3, newSize3);
    printf("Size after:            %d\n", newSize3);
    printf("Duplicates removed:    %d\n\n", n3 - newSize3);
    
    // TEST CASE 4: No duplicates
    printf("TEST CASE 4: No duplicates (random order)\n");
    printf("--------------------------------------\n");
    int arr4[] = {5, 2, 8, 1, 9, 3, 7};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    
    printf("Original array:        ");
    printArray(arr4, n4);
    printf("Size before:           %d\n", n4);
    
    int newSize4 = removeDuplicates_SortFirst(arr4, n4);
    
    printf("After sorting & removing: ");
    printArray(arr4, newSize4);
    printf("Size after:            %d\n", newSize4);
    printf("Duplicates removed:    0\n\n");
    
    // TEST CASE 5: Negative numbers
    printf("TEST CASE 5: Array with negative numbers\n");
    printf("--------------------------------------\n");
    int arr5[] = {-5, 3, -5, 0, 3, -2, 0, 5};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    
    printf("Original array:        ");
    printArray(arr5, n5);
    printf("Size before:           %d\n", n5);
    
    int newSize5 = removeDuplicates_SortFirst(arr5, n5);
    
    printf("After sorting & removing: ");
    printArray(arr5, newSize5);
    printf("Size after:            %d\n", newSize5);
    printf("Result is SORTED:      Yes\n", newSize5);
    printf("Duplicates removed:    %d\n\n", n5 - newSize5);
    
    // TEST CASE 6: Large numbers with duplicates
    printf("TEST CASE 6: Large numbers with duplicates\n");
    printf("----------------------------------------\n");
    int arr6[] = {100, 50, 100, 75, 50, 25, 75};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    
    printf("Original array:        ");
    printArray(arr6, n6);
    printf("Size before:           %d\n", n6);
    
    int newSize6 = removeDuplicates_SortFirst(arr6, n6);
    
    printf("After sorting & removing: ");
    printArray(arr6, newSize6);
    printf("Size after:            %d\n", newSize6);
    printf("Result is SORTED:      Yes\n");
    printf("Duplicates removed:    %d\n\n", n6 - newSize6);
    
    // TEST CASE 7: Two element array
    printf("TEST CASE 7: Two element array (both duplicates)\n");
    printf("----------------------------------------------\n");
    int arr7[] = {42, 42};
    int n7 = sizeof(arr7) / sizeof(arr7[0]);
    
    printf("Original array:        ");
    printArray(arr7, n7);
    printf("Size before:           %d\n", n7);
    
    int newSize7 = removeDuplicates_SortFirst(arr7, n7);
    
    printf("After sorting & removing: ");
    printArray(arr7, newSize7);
    printf("Size after:            %d\n", newSize7);
    printf("Duplicates removed:    %d\n\n", n7 - newSize7);
    
    // TEST CASE 8: Single element
    printf("TEST CASE 8: Single element\n");
    printf("--------------------------\n");
    int arr8[] = {99};
    int n8 = sizeof(arr8) / sizeof(arr8[0]);
    
    printf("Original array:        ");
    printArray(arr8, n8);
    printf("Size before:           %d\n", n8);
    
    int newSize8 = removeDuplicates_SortFirst(arr8, n8);
    
    printf("After sorting & removing: ");
    printArray(arr8, newSize8);
    printf("Size after:            %d\n\n", newSize8);
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n log n) - Sort dominates\n");
    printf("  Space Complexity: O(1)  - In-place (except O(log n) recursion)\n");
    printf("  Pros:  Good balance, works with any array, result sorted\n");
    printf("  Cons:  Changes order (sorts data)\n");
    printf("  Best Use: General-purpose, most practical for real-world\n");
    printf("================================================================================\n");
    
    return 0;
}
