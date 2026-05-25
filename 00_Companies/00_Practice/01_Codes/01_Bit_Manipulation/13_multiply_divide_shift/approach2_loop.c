/* Approach 2: Loop-based (Educational) */
#include <stdio.h>

int multiply_by_power_of_2(int n, int power) {
    for (int i = 0; i < power; i++)
        n += n;  // Double each time
    return n;
}

int divide_by_power_of_2(int n, int power) {
    for (int i = 0; i < power; i++)
        n /= 2;  // Halve each time
    return n;
}

int main() {
    int num = 10;
    
    printf("Original: %d\n\n", num);
    
    printf("Multiply:\n");
    printf("%d * 2^2 = %d\n", num, multiply_by_power_of_2(num, 2));
    printf("%d * 2^3 = %d\n", num, multiply_by_power_of_2(num, 3));
    
    printf("\nDivide:\n");
    printf("%d / 2^1 = %d\n", num, divide_by_power_of_2(num, 1));
    printf("%d / 2^2 = %d\n", num, divide_by_power_of_2(num, 2));
    
    return 0;
}
