/*
================================================================================
                   METHOD 4: HASH ARRAY APPROACH
================================================================================

EXPLANATION:
-----------
This method uses a frequency array (hash table) to count occurrences of elements.
It's very efficient when the range of integers is small and known.
It works by marking which numbers appear in the array, then collecting them.

HOW IT WORKS:
1. Create a frequency array of size (maxValue + 1)
   - Each index represents a number
   - Value at index represents how many times it appears
2. Initialize all frequencies to 0 (calloc does this)
3. For each element in original array:
   - Increment frequency at that element's index
4. For each index in frequency array:
   - If frequency > 0, add index to result
5. Return count of unique elements
   Note: Result will be in sorted order!

EXAMPLE:
  Original: [5, 2, 8, 1, 5, 2, 10, 1, 3], maxVal = 10
  
  Create freq[11]: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
                    0  1  2  3  4  5  6  7  8  9  10
  
  Mark frequencies:
    5 → freq[5]++  → [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0]
    2 → freq[2]++  → [0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0]
    8 → freq[8]++  → [0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0]
    1 → freq[1]++  → [0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0]
    5 → freq[5]++  → [0, 1, 1, 0, 0, 2, 0, 0, 1, 0, 0]
    2 → freq[2]++  → [0, 1, 2, 0, 0, 2, 0, 0, 1, 0, 0]
    10→ freq[10]++ → [0, 1, 2, 0, 0, 2, 0, 0, 1, 0, 1]
    1 → freq[1]++  → [0, 2, 2, 0, 0, 2, 0, 0, 1, 0, 1]
    3 → freq[3]++  → [0, 2, 2, 1, 0, 2, 0, 0, 1, 0, 1]
  
  Collect elements with freq > 0:
    freq[1]=2 > 0 → Add 1
    freq[2]=2 > 0 → Add 2
    freq[3]=1 > 0 → Add 3
    freq[5]=2 > 0 → Add 5
    freq[8]=1 > 0 → Add 8
    freq[10]=1 > 0 → Add 10
  
  Result: [1, 2, 3, 5, 8, 10] (sorted, unique)

TIME COMPLEXITY: O(n + maxVal)
  - First loop: n operations to mark frequencies
  - Second loop: maxVal operations to collect results
  - Total: O(n + maxVal)
  - If maxVal is small, effectively O(n)

SPACE COMPLEXITY: O(maxVal)
  - Frequency array of size maxVal + 1
  - Not dependent on array size n

WHEN TO USE:
  ✓ Number range is small and known (0-100, 0-1000)
  ✓ Need very fast execution O(n)
  ✓ Space available for frequency array
  ✓ Sorted result is acceptable
  ✓ Need to count occurrences anyway

WHEN NOT TO USE:
  ✗ Range is very large (0-1,000,000,000)
  ✗ Memory is extremely limited
  ✗ Need to preserve original order
  ✗ Negative numbers (need offset adjustment)
  ✗ Floating point numbers (won't work)

ADVANTAGES:
  + Fastest O(n) time complexity for known range
  + Very simple and elegant
  + Result is automatically sorted
  + Can easily count frequencies if needed
  + Great for contest programming

DISADVANTAGES:
  - Requires knowing maximum value
  - Uses extra space O(maxVal)
  - Only works for non-negative integers (or with offset)
  - Not suitable for sparse data
  - Not suitable for floating point numbers

REAL-WORLD ANALOGY:
Think of it like a library's shelving system:
- Create shelves numbered 0 to maxValue
- Put each book on its corresponding shelf
- Mark which shelves have books
- Collect one book from each occupied shelf

================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Function: removeDuplicates_Hash
Purpose: Remove duplicates using frequency array (hash table approach)
Parameters:
  - arr: pointer to integer array (values 0 to maxVal)
  - n: number of elements in array
  - result: pointer to result array where unique elements will be stored
  - maxVal: maximum value expected in array (size of frequency array)
Returns:
  - Count of unique elements
*/
int removeDuplicates_Hash(int arr[], int n, int result[], int maxVal) {
    /*
    Create frequency array dynamically
    calloc initializes all values to 0
    This array will store frequency of each number from 0 to maxVal
    */
    int *freq = (int *)calloc(maxVal + 1, sizeof(int));
    int count = 0;  // Counter for unique elements
    
    /*
    STEP 1: Mark frequencies
    For each element in array, increment its frequency
    */
    for (int i = 0; i < n; i++) {
        /*
        Check if element is in valid range (0 to maxVal)
        */
        if (arr[i] >= 0 && arr[i] <= maxVal) {
            freq[arr[i]]++;  // Increment frequency for this element
        }
    }
    
    /*
    STEP 2: Collect unique elements
    Go through frequency array and collect elements with frequency > 0
    This automatically gives us sorted unique elements
    */
    for (int i = 0; i <= maxVal; i++) {
        /*
        If this element appears at least once in array
        */
        if (freq[i] > 0) {
            result[count] = i;  // Add element to result
            count++;  // Increment count of unique elements
        }
    }
    
    /*
    Free the dynamically allocated memory
    */
    free(freq);
    
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
Purpose: Test the hash array method with examples
*/
int main() {
    printf("================================================================================\n");
    printf("              METHOD 4: HASH ARRAY APPROACH - TEST CASES\n");
    printf("================================================================================\n\n");
    
    // TEST CASE 1: Array with range 0-10
    printf("TEST CASE 1: Array with range 0-10 (maxVal=10)\n");
    printf("----------------------------------------------\n");
    int arr1[] = {5, 2, 8, 1, 5, 2, 10, 1, 3};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    int result1[100];
    int maxVal1 = 10;
    
    printf("Original array:        ");
    printArray(arr1, n1);
    printf("Max value:             %d\n", maxVal1);
    printf("Size:                  %d\n", n1);
    
    int newSize1 = removeDuplicates_Hash(arr1, n1, result1, maxVal1);
    
    printf("After removing duplicates: ");
    printArray(result1, newSize1);
    printf("Size:                  %d\n", newSize1);
    printf("Result is SORTED:      Yes (automatic)\n");
    printf("Duplicates removed:    %d\n\n", n1 - newSize1);
    
    // TEST CASE 2: All duplicates (maxVal=5)
    printf("TEST CASE 2: Array where all elements are same (maxVal=5)\n");
    printf("-------------------------------------------------------\n");
    int arr2[] = {3, 3, 3, 3, 3};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    int result2[100];
    int maxVal2 = 5;
    
    printf("Original array:        ");
    printArray(arr2, n2);
    printf("Max value:             %d\n", maxVal2);
    printf("Size:                  %d\n", n2);
    
    int newSize2 = removeDuplicates_Hash(arr2, n2, result2, maxVal2);
    
    printf("After removing duplicates: ");
    printArray(result2, newSize2);
    printf("Size:                  %d\n", newSize2);
    printf("Unique elements:       %d\n\n", newSize2);
    
    // TEST CASE 3: No duplicates (maxVal=20)
    printf("TEST CASE 3: Array with no duplicates (maxVal=20)\n");
    printf("-------------------------------------------------\n");
    int arr3[] = {5, 12, 3, 8, 20, 1, 15};
    int n3 = sizeof(arr3) / sizeof(arr3[0]);
    int result3[100];
    int maxVal3 = 20;
    
    printf("Original array:        ");
    printArray(arr3, n3);
    printf("Max value:             %d\n", maxVal3);
    printf("Size:                  %d\n", n3);
    
    int newSize3 = removeDuplicates_Hash(arr3, n3, result3, maxVal3);
    
    printf("After removing duplicates: ");
    printArray(result3, newSize3);
    printf("Size:                  %d\n", newSize3);
    printf("Result is SORTED:      Yes (auto-sorted by index)\n\n");
    
    // TEST CASE 4: Range 0-15
    printf("TEST CASE 4: Unsorted array with range 0-15 (maxVal=15)\n");
    printf("------------------------------------------------------\n");
    int arr4[] = {15, 5, 10, 5, 2, 10, 8, 2, 0, 15};
    int n4 = sizeof(arr4) / sizeof(arr4[0]);
    int result4[100];
    int maxVal4 = 15;
    
    printf("Original array:        ");
    printArray(arr4, n4);
    printf("Max value:             %d\n", maxVal4);
    printf("Size:                  %d\n", n4);
    
    int newSize4 = removeDuplicates_Hash(arr4, n4, result4, maxVal4);
    
    printf("After removing duplicates: ");
    printArray(result4, newSize4);
    printf("Size:                  %d\n", newSize4);
    printf("Result is SORTED:      Yes\n", newSize4);
    printf("Duplicates removed:    %d\n\n", n4 - newSize4);
    
    // TEST CASE 5: Single unique element
    printf("TEST CASE 5: Single element array (maxVal=10)\n");
    printf("--------------------------------------------\n");
    int arr5[] = {7};
    int n5 = sizeof(arr5) / sizeof(arr5[0]);
    int result5[100];
    int maxVal5 = 10;
    
    printf("Original array:        ");
    printArray(arr5, n5);
    printf("Max value:             %d\n", maxVal5);
    printf("Size:                  %d\n", n5);
    
    int newSize5 = removeDuplicates_Hash(arr5, n5, result5, maxVal5);
    
    printf("After removing duplicates: ");
    printArray(result5, newSize5);
    printf("Size:                  %d\n\n", newSize5);
    
    // TEST CASE 6: Larger range (maxVal=50)
    printf("TEST CASE 6: Larger array with range 0-50 (maxVal=50)\n");
    printf("----------------------------------------------------\n");
    int arr6[] = {45, 10, 35, 10, 5, 45, 25, 5, 50, 50, 20};
    int n6 = sizeof(arr6) / sizeof(arr6[0]);
    int result6[100];
    int maxVal6 = 50;
    
    printf("Original array:        ");
    printArray(arr6, n6);
    printf("Max value:             %d\n", maxVal6);
    printf("Size:                  %d\n", n6);
    
    int newSize6 = removeDuplicates_Hash(arr6, n6, result6, maxVal6);
    
    printf("After removing duplicates: ");
    printArray(result6, newSize6);
    printf("Size:                  %d\n", newSize6);
    printf("Result is SORTED:      Yes\n");
    printf("Duplicates removed:    %d\n\n", n6 - newSize6);
    
    // TEST CASE 7: Sequential numbers
    printf("TEST CASE 7: Sequential numbers with duplicates (maxVal=10)\n");
    printf("-----------------------------------------------------------\n");
    int arr7[] = {1, 2, 3, 2, 1, 4, 5, 4, 3, 6};
    int n7 = sizeof(arr7) / sizeof(arr7[0]);
    int result7[100];
    int maxVal7 = 10;
    
    printf("Original array:        ");
    printArray(arr7, n7);
    printf("Max value:             %d\n", maxVal7);
    printf("Size:                  %d\n", n7);
    
    int newSize7 = removeDuplicates_Hash(arr7, n7, result7, maxVal7);
    
    printf("After removing duplicates: ");
    printArray(result7, newSize7);
    printf("Size:                  %d\n", newSize7);
    printf("Result is SORTED:      Yes (1, 2, 3, 4, 5, 6)\n\n", newSize7);
    
    printf("================================================================================\n");
    printf("SUMMARY:\n");
    printf("  Time Complexity:  O(n + maxVal) - Typically O(n) if maxVal is small\n");
    printf("  Space Complexity: O(maxVal) - Frequency array size\n");
    printf("  Pros:  Fastest method for small ranges, result is sorted\n");
    printf("  Cons:  Requires knowing max value, not suitable for large ranges\n");
    printf("  Best Use: Small integer ranges, performance critical code\n");
    printf("================================================================================\n");
    
    return 0;
}
