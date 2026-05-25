/* Approach 1: Right Shift Count */
#include <stdio.h>

int log2_bitwise(unsigned int n) {
    if (n == 0) return -1;
    
    int log = 0;
    while (n >>= 1)
        log++;
    return log;
}

int main() {
    unsigned int nums[] = {1, 2, 4, 8, 16, 32, 100, 1024};
    
    for (int i = 0; i < 8; i++) {
        printf("log2(%u) = %d\n", nums[i], log2_bitwise(nums[i]));
    }
    return 0;
}
