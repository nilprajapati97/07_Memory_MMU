/* Approach 2: Array of Function Pointers */
#include <stdio.h>

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int divide(int a, int b) { return b ? a / b : 0; }

int main() {
    // Array of function pointers
    int (*operations[4])(int, int) = {add, sub, mul, divide};
    const char *names[] = {"Add", "Sub", "Mul", "Div"};
    
    int a = 20, b = 5;
    
    // Call all operations using array
    for (int i = 0; i < 4; i++) {
        printf("%s(%d, %d) = %d\n", names[i], a, b, operations[i](a, b));
    }
    
    return 0;
}
