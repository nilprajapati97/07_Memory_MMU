/* Approach 3: GCC Builtin (Best) */
#include <stdio.h>

int msb_position(unsigned int n) {
    if (n == 0) return -1;
    return 31 - __builtin_clz(n);
}

int main() {
    unsigned int nums[] = {0, 1, 10, 15, 255, 1024, 0xFFFFFFFF};
    
    for (int i = 0; i < 7; i++) {
        int pos = msb_position(nums[i]);
        if (pos != -1)
            printf("0x%X: MSB at position %d\n", nums[i], pos);
        else
            printf("0x%X: No bits set\n", nums[i]);
    }
    
    // Also available:
    // __builtin_clzl() for long
    // __builtin_clzll() for long long
    // __builtin_ctz() for trailing zeros
    
    return 0;
}
