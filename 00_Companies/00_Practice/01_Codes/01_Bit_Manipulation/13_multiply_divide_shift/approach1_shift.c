/* Approach 1: Direct Shift (Best) */
#include <stdio.h>

int multiply_by_power_of_2(int n, int power) {
    return n << power;
}

int divide_by_power_of_2(int n, int power) {
    return n >> power;
}

int main() {
    int num = 10;
    
    printf("Original: %d\n\n", num);
    
    printf("Multiply:\n");
    printf("%d * 2^1 = %d\n", num, multiply_by_power_of_2(num, 1));
    printf("%d * 2^2 = %d\n", num, multiply_by_power_of_2(num, 2));
    printf("%d * 2^3 = %d\n", num, multiply_by_power_of_2(num, 3));
    
    printf("\nDivide:\n");
    printf("%d / 2^1 = %d\n", num, divide_by_power_of_2(num, 1));
    printf("%d / 2^2 = %d\n", num, divide_by_power_of_2(num, 2));
    
    return 0;
}
