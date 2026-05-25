/* Approach 1: Right Shift Count */
#include <stdio.h>

int msb_position(unsigned int n) {
    if (n == 0) return -1;
    
    int pos = 0;
    while (n >>= 1)
        pos++;
    return pos;
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
    return 0;
}
