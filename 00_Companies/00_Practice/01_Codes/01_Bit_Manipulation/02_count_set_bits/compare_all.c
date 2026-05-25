/* Compare All Approaches
 * Demonstrates all methods side-by-side for comparison
 */
#include <stdio.h>

// Approach 1: Naive - O(log n)
int naive(int n) {
    int count = 0;
    while (n) {
        count += (n & 1);
        n >>= 1;
    }
    return count;
}

// Approach 2: Brian Kernighan - O(k) where k = set bits
int kernighan(int n) {
    int count = 0;
    while (n) {
        n &= (n - 1);  // Clear rightmost set bit
        count++;
    }
    return count;
}

// Approach 3: Builtin - O(1) hardware instruction
int builtin(int n) {
    return __builtin_popcount(n);
}

// Approach 4: Recursive - O(log n)
int recursive(int n) {
    return n ? (n & 1) + recursive(n >> 1) : 0;
}

int main(void) {
    int test[] = {0, 7, 15, 255, 1023, 0x7FFFFFFF};
    int size = sizeof(test) / sizeof(test[0]);
    
    printf("Number\t\tNaive\tKernighan\tBuiltin\t\tRecursive\n");
    printf("------\t\t-----\t---------\t-------\t\t---------\n");
    
    for (int i = 0; i < size; i++) {
        printf("%d\t\t%d\t%d\t\t%d\t\t%d\n",
               test[i],
               naive(test[i]),
               kernighan(test[i]),
               builtin(test[i]),
               recursive(test[i]));
    }
    
    return 0;
}
