/* Approach 3: GCC Builtin (Best) */
#include <stdio.h>

int log2_builtin(unsigned int n) {
    if (n == 0) return -1;
    return 31 - __builtin_clz(n);
}

int main() {
    unsigned int nums[] = {1, 2, 4, 8, 16, 32, 100, 1024};
    
    for (int i = 0; i < 8; i++) {
        printf("log2(%u) = %d\n", nums[i], log2_builtin(nums[i]));
    }
    
    // Also available:
    // __builtin_clzl() for long
    // __builtin_clzll() for long long
    
    return 0;
}
