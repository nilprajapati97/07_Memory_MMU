/* Approach 2: Find MSB Position */
#include <stdio.h>

int log2_msb(unsigned int n) {
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
    unsigned int nums[] = {1, 2, 4, 8, 16, 32, 100, 1024};
    
    for (int i = 0; i < 8; i++) {
        printf("log2(%u) = %d\n", nums[i], log2_msb(nums[i]));
    }
    return 0;
}
