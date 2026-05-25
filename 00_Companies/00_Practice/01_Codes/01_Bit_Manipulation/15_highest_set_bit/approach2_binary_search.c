/* Approach 2: Binary Search (Lookup) */
#include <stdio.h>

int msb_position(unsigned int n) {
    if (n == 0) return -1;
    
    int pos = 0;
    if (n & 0xFFFF0000) { pos += 16; n >>= 16; }
    if (n & 0xFF00) { pos += 8; n >>= 8; }
    if (n & 0xF0) { pos += 4; n >>= 4; }
    if (n & 0xC) { pos += 2; n >>= 2; }
    if (n & 0x2) { pos += 1; }
    
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
