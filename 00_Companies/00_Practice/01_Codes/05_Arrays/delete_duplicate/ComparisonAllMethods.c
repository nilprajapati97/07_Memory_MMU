/*
================================================================================
                 COMPARISON OF ALL 6 METHODS - UNIFIED
================================================================================

This file demonstrates all 6 methods side by side on the same test data.
This makes it easy to compare:
  1. Output differences
  2. Performance characteristics
  3. Complexity analysis
  4. When to use each method

================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
   METHOD 1: SIMPLE SHIFTING
   ============================================================================ */

int method1_SimpleShift(int arr[], int n) {
    int i, j, k;
    int newSize = n;
    
    for (i = 0; i < newSize; i++) {
        for (j = i + 1; j < newSize; j++) {
            if (arr[i] == arr[j]) {
                for (k = j; k < newSize - 1; k++) {
                    arr[k] = arr[k + 1];
                }
                newSize--;
                j--;
            }
        }
    }
    return newSize;
}

/* ============================================================================
   METHOD 2: SORTED ARRAY
   ============================================================================ */

int method2_SortedArray(int arr[], int n) {
    if (n <= 1)
        return n;
    
    // Assumes array is already sorted for this demo
    int writeIndex = 0;
    
    for (int i = 1; i < n; i++) {
        if (arr[i] != arr[writeIndex]) {
            writeIndex++;
            arr[writeIndex] = arr[i];
        }
    }
    return writeIndex + 1;
}

/* ============================================================================
   METHOD 3: EXTRA ARRAY
   ============================================================================ */

int method3_ExtraArray(int arr[], int n, int result[]) {
    int count = 0;
    int isDuplicate;
    
    for (int i = 0; i < n; i++) {
        isDuplicate = 0;
        for (int j = 0; j < count; j++) {
            if (arr[i] == result[j]) {
                isDuplicate = 1;
                break;
            }
        }
        if (!isDuplicate) {
            result[count++] = arr[i];
        }
    }
    return count;
}

/* ============================================================================
   METHOD 4: HASH ARRAY
   ============================================================================ */

int method4_HashArray(int arr[], int n, int result[], int maxVal) {
    int *freq = (int *)calloc(maxVal + 1, sizeof(int));
    int count = 0;
    
    for (int i = 0; i < n; i++) {
        if (arr[i] >= 0 && arr[i] <= maxVal) {
            freq[arr[i]]++;
        }
    }
    
    for (int i = 0; i <= maxVal; i++) {
        if (freq[i] > 0) {
            result[count++] = i;
        }
    }
    
    free(freq);
    return count;
}

/* ============================================================================
   METHOD 5: SORT-FIRST
   ============================================================================ */

int partition(int arr[], int low, int high) {
    int pivot = arr[high];
    int i = low - 1;
    
    for (int j = low; j < high; j++) {
        if (arr[j] < pivot) {
            i++;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    return i + 1;
}

void quickSort(int arr[], int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

int method5_SortFirst(int arr[], int n) {
    if (n <= 1)
        return n;
    
    quickSort(arr, 0, n - 1);
    
    int writeIndex = 0;
    for (int i = 1; i < n; i++) {
        if (arr[i] != arr[writeIndex]) {
            writeIndex++;
            arr[writeIndex] = arr[i];
        }
    }
    return writeIndex + 1;
}

/* ============================================================================
   METHOD 6: ORDER-PRESERVING
   ============================================================================ */

int method6_OrderPreserve(int arr[], int n, int result[]) {
    int *visited = (int *)calloc(n, sizeof(int));
    int resultCount = 0;
    
    for (int i = 0; i < n; i++) {
        int isDuplicate = 0;
        for (int j = 0; j < i; j++) {
            if (arr[i] == arr[j] && visited[j]) {
                isDuplicate = 1;
                break;
            }
        }
        if (!isDuplicate) {
            result[resultCount++] = arr[i];
            visited[i] = 1;
        }
    }
    
    free(visited);
    return resultCount;
}

/* ============================================================================
   UTILITY FUNCTIONS
   ============================================================================ */

void printArray(int arr[], int n) {
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]");
}

void copyArray(int source[], int dest[], int n) {
    for (int i = 0; i < n; i++) {
        dest[i] = source[i];
    }
}

/* ============================================================================
   MAIN - TEST ALL METHODS
   ============================================================================ */

int main() {
    printf("================================================================================\n");
    printf("                 COMPARISON OF ALL 6 METHODS\n");
    printf("================================================================================\n\n");
    
    // Original unsorted array
    int original[] = {5, 3, 5, 1, 3, 2, 1, 4};
    int n = sizeof(original) / sizeof(original[0]);
    int temp_result[100];
    
    printf("ORIGINAL ARRAY: ");
    printArray(original, n);
    printf(" (size: %d)\n\n", n);
    
    // ========================================================================
    // METHOD 1: SIMPLE SHIFTING
    // ========================================================================
    printf("METHOD 1: SIMPLE SHIFTING\n");
    printf("------------------------\n");
    copyArray(original, temp_result, n);
    int size1 = method1_SimpleShift(temp_result, n);
    printf("Result:   ");
    printArray(temp_result, size1);
    printf(" (size: %d)\n", size1);
    printf("Time:     O(n²)\n");
    printf("Space:    O(1)\n");
    printf("Pros:     Simple, in-place\n");
    printf("Cons:     Very slow for large arrays\n");
    printf("Best for: Learning, very small arrays\n\n");
    
    // ========================================================================
    // METHOD 2: SORTED ARRAY
    // ========================================================================
    printf("METHOD 2: SORTED ARRAY (Requires pre-sorted)\n");
    printf("-------------------------------------------\n");
    copyArray(original, temp_result, n);
    quickSort(temp_result, 0, n - 1);  // First sort it
    printf("After sorting: ");
    printArray(temp_result, n);
    printf("\n");
    int size2 = method2_SortedArray(temp_result, n);
    printf("After dedup:   ");
    printArray(temp_result, size2);
    printf(" (size: %d)\n", size2);
    printf("Time:     O(n)   [if pre-sorted]\n");
    printf("Space:    O(1)\n");
    printf("Pros:     Very fast for sorted data, in-place\n");
    printf("Cons:     Requires sorted input\n");
    printf("Best for: Pre-sorted arrays\n\n");
    
    // ========================================================================
    // METHOD 3: EXTRA ARRAY
    // ========================================================================
    printf("METHOD 3: EXTRA ARRAY\n");
    printf("---------------------\n");
    copyArray(original, temp_result, n);
    int result3[100];
    int size3 = method3_ExtraArray(temp_result, n, result3);
    printf("Result:   ");
    printArray(result3, size3);
    printf(" (size: %d)\n", size3);
    printf("Time:     O(n²)\n");
    printf("Space:    O(n)\n");
    printf("Pros:     Preserves order, simple logic\n");
    printf("Cons:     O(n²) time, uses extra space\n");
    printf("Best for: When order preservation matters\n\n");
    
    // ========================================================================
    // METHOD 4: HASH ARRAY
    // ========================================================================
    printf("METHOD 4: HASH ARRAY (maxVal=5)\n");
    printf("-------------------------------\n");
    copyArray(original, temp_result, n);
    int result4[100];
    int size4 = method4_HashArray(temp_result, n, result4, 5);
    printf("Result:   ");
    printArray(result4, size4);
    printf(" (size: %d)\n", size4);
    printf("Time:     O(n + maxVal)\n");
    printf("Space:    O(maxVal)\n");
    printf("Pros:     Fastest O(n), result sorted\n");
    printf("Cons:     Need to know max value, limited range\n");
    printf("Best for: Small integer range\n\n");
    
    // ========================================================================
    // METHOD 5: SORT-FIRST
    // ========================================================================
    printf("METHOD 5: SORT-FIRST\n");
    printf("-------------------\n");
    copyArray(original, temp_result, n);
    int size5 = method5_SortFirst(temp_result, n);
    printf("Result:   ");
    printArray(temp_result, size5);
    printf(" (size: %d)\n", size5);
    printf("Time:     O(n log n)\n");
    printf("Space:    O(1)  [except O(log n) recursion]\n");
    printf("Pros:     Good balance, practical, in-place\n");
    printf("Cons:     Changes order (sorts data)\n");
    printf("Best for: General-purpose, most practical\n\n");
    
    // ========================================================================
    // METHOD 6: ORDER-PRESERVING
    // ========================================================================
    printf("METHOD 6: ORDER-PRESERVING\n");
    printf("--------------------------\n");
    copyArray(original, temp_result, n);
    int result6[100];
    int size6 = method6_OrderPreserve(temp_result, n, result6);
    printf("Result:   ");
    printArray(result6, size6);
    printf(" (size: %d)\n", size6);
    printf("Time:     O(n²)\n");
    printf("Space:    O(n)\n");
    printf("Pros:     Preserves order, flexible\n");
    printf("Cons:     O(n²) time, uses extra space\n");
    printf("Best for: When original order matters\n\n");
    
    // ========================================================================
    // COMPARISON TABLE
    // ========================================================================
    printf("================================================================================\n");
    printf("COMPREHENSIVE COMPARISON TABLE\n");
    printf("================================================================================\n\n");
    
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "Method", "Time", "Space", "In-Place", "Sorted Input", "Sorted Output");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "------", "----", "-----", "--------", "------------", "-------------");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "1.Simple", "O(n²)", "O(1)", "Yes", "No", "No");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "2.Sorted", "O(n)", "O(1)", "Yes", "YES", "Yes");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "3.Extra", "O(n²)", "O(n)", "No", "No", "No");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "4.Hash", "O(n)", "O(m)", "No", "No", "YES");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "5.Sort", "O(nlogn)", "O(1)", "Yes", "No", "YES");
    printf("%-8s %-10s %-10s %-10s %-15s %-15s\n",
           "6.Order", "O(n²)", "O(n)", "No", "No", "No");
    
    printf("\n");
    printf("Legend: m = maxValue, n = array size\n");
    printf("        In-Place = No extra array needed\n");
    printf("        Sorted Input = Requires data to be sorted\n");
    printf("        Sorted Output = Result comes out sorted\n\n");
    
    // ========================================================================
    // DECISION GUIDE
    // ========================================================================
    printf("================================================================================\n");
    printf("DECISION GUIDE - WHICH METHOD TO CHOOSE?\n");
    printf("================================================================================\n\n");
    
    printf("1. Is array already SORTED?\n");
    printf("   └─ YES → Use METHOD 2 (Sorted Array) [O(n)]\n");
    printf("   └─ NO  → Continue to question 2\n\n");
    
    printf("2. Is memory VERY LIMITED?\n");
    printf("   └─ YES → Use METHOD 5 (Sort-First) [O(n log n), O(1)]\n");
    printf("   └─ NO  → Continue to question 3\n\n");
    
    printf("3. Is range of integers SMALL (<100)?\n");
    printf("   └─ YES → Use METHOD 4 (Hash Array) [O(n)]\n");
    printf("   └─ NO  → Continue to question 4\n\n");
    
    printf("4. Must ORDER BE PRESERVED?\n");
    printf("   └─ YES → Use METHOD 3 or 6 (Order-Preserving) [O(n²)]\n");
    printf("   └─ NO  → Use METHOD 5 (Sort-First) [O(n log n)]\n\n");
    
    printf("================================================================================\n");
    printf("REAL-WORLD RECOMMENDATIONS\n");
    printf("================================================================================\n\n");
    
    printf("Small Array (< 100 elements):\n");
    printf("  - Method 1 (Simple) is fine for learning\n");
    printf("  - Method 5 (Sort-First) for production\n\n");
    
    printf("Medium Array (100-10,000 elements):\n");
    printf("  - Method 5 (Sort-First) if order doesn't matter [RECOMMENDED]\n");
    printf("  - Method 4 (Hash) if range is small [FASTEST]\n");
    printf("  - Method 3/6 if order must be preserved\n\n");
    
    printf("Large Array (> 10,000 elements):\n");
    printf("  - Method 4 (Hash) if range is small [FASTEST]\n");
    printf("  - Method 5 (Sort-First) general purpose [RECOMMENDED]\n");
    printf("  - Method 2 (Sorted) only if already sorted\n\n");
    
    printf("Performance-Critical (e.g., real-time system):\n");
    printf("  - Method 4 (Hash) if possible [FASTEST]\n");
    printf("  - Method 5 (Sort-First) if not [PRACTICAL]\n\n");
    
    printf("Order Preservation Required:\n");
    printf("  - Method 3 or 6 (Order-Preserving)\n");
    printf("  - Consider using hash table instead for O(n)\n\n");
    
    printf("================================================================================\n");
    printf("LEARNING PATH\n");
    printf("================================================================================\n\n");
    
    printf("Beginner:     Start with METHOD 1 (Simple Shifting)\n");
    printf("              Understand the basic concept\n");
    printf("              Analyze time/space complexity\n\n");
    
    printf("Intermediate: Learn METHOD 2 (Sorted Array)\n");
    printf("              Understand advantage of sorted input\n");
    printf("              Compare with METHOD 1\n\n");
    
    printf("Advanced:     Learn METHOD 4 & 5\n");
    printf("              Understand trade-offs\n");
    printf("              Learn to choose based on constraints\n\n");
    
    printf("Expert:       Learn METHOD 3 & 6\n");
    printf("              Understand order preservation\n");
    printf("              Master all approaches\n\n");
    
    printf("================================================================================\n");
    
    return 0;
}
