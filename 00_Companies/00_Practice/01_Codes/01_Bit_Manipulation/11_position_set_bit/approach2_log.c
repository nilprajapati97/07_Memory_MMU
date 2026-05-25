/* Approach 2: Using Log2 */
#include <stdio.h>
#include <math.h>

int find_position(unsigned int n) {
    if (n == 0 || (n & (n - 1)))
        return -1;
    
    return (int)log2(n);
}

int main() {
    unsigned int nums[] = {0, 1, 2, 4, 8, 16, 18};
    
    for (int i = 0; i < 7; i++) {
        int pos = find_position(nums[i]);
        if (pos != -1)
            printf("%u: Position %d\n", nums[i], pos);
        else
            printf("%u: Not a single bit\n", nums[i]);
    }
    return 0;
}
