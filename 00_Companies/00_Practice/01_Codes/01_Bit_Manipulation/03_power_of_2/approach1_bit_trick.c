/* Approach 1: Bit Trick (Best) */
#include <stdio.h>

int is_power_of_2(unsigned int n) {
    return n && !(n & (n - 1));
}

int main() {
    unsigned int nums[] = {0, 1, 2, 3, 4, 16, 18, 32, 64};
    
    for (int i = 0; i < 9; i++) {
        printf("%u: %s\n", nums[i], 
               is_power_of_2(nums[i]) ? "Power of 2" : "Not power of 2");
    }
    return 0;
}
