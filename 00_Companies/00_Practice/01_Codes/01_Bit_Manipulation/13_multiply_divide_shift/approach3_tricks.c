/* Approach 3: Optimization Tricks */
#include <stdio.h>

// Multiply by 3: n * 3 = n * 2 + n = (n << 1) + n
int multiply_by_3(int n) {
    return (n << 1) + n;
}

// Multiply by 5: n * 5 = n * 4 + n = (n << 2) + n
int multiply_by_5(int n) {
    return (n << 2) + n;
}

// Multiply by 7: n * 7 = n * 8 - n = (n << 3) - n
int multiply_by_7(int n) {
    return (n << 3) - n;
}

// Divide by 2 with rounding
int divide_by_2_round(int n) {
    return (n + 1) >> 1;
}

int main() {
    int num = 10;
    
    printf("Original: %d\n\n", num);
    
    printf("Fast multiply:\n");
    printf("%d * 3 = %d\n", num, multiply_by_3(num));
    printf("%d * 5 = %d\n", num, multiply_by_5(num));
    printf("%d * 7 = %d\n", num, multiply_by_7(num));
    
    printf("\nDivide with rounding:\n");
    printf("%d / 2 (rounded) = %d\n", 11, divide_by_2_round(11));
    
    return 0;
}
