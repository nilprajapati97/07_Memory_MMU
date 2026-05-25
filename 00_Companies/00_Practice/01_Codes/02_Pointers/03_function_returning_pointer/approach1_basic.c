/* Function Returning Pointer to Function */
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

// Function that returns a function pointer
int (*get_operation(char op))(int, int) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        case '*': return mul;
        default: return NULL;
    }
}

// Using typedef for clarity
typedef int (*operation_t)(int, int);

operation_t get_operation_v2(char op) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        case '*': return mul;
        default: return NULL;
    }
}

int main() {
    // Get function pointer and use it
    int (*op)(int, int) = get_operation('+');
    if (op) {
        printf("10 + 5 = %d\n", op(10, 5));
    }
    
    // Using typedef version
    operation_t op2 = get_operation_v2('*');
    if (op2) {
        printf("10 * 5 = %d\n", op2(10, 5));
    }
    
    // Direct call
    printf("10 - 5 = %d\n", get_operation('-')(10, 5));
    
    return 0;
}
