/* Approach 2: Count Set Bits */
#include <stdio.h>

int count_set_bits(unsigned int n) {
    int count = 0;
    while (n) {
        n &= (n - 1);
        count++;
    }
    return count;
}

int is_power_of_2(unsigned int n) {
    return n && count_set_bits(n) == 1;
}

int main() {
    unsigned int nums[] = {0, 1, 2, 3, 4, 16, 18, 32, 64};
    
    for (int i = 0; i < 9; i++) {
        printf("%u: %s\n", nums[i], 
               is_power_of_2(nums[i]) ? "Power of 2" : "Not power of 2");
    }
    return 0;
}
