/* Approach 1: Basic Function Pointers */
#include <stdio.h>

// Simple functions
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divide(int a, int b) { return b ? a / b : 0; }

int main() {
    // Function pointer declaration
    int (*operation)(int, int);
    
    // Assign and call
    operation = add;
    printf("10 + 5 = %d\n", operation(10, 5));
    
    operation = sub;
    printf("10 - 5 = %d\n", operation(10, 5));
    
    operation = mul;
    printf("10 * 5 = %d\n", operation(10, 5));
    
    operation = divide;
    printf("10 / 5 = %d\n", operation(10, 5));
    
    return 0;
}
