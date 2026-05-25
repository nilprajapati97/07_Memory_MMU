/* Approach 3: GCC Builtin (x86) */
#include <stdio.h>

// Note: These may not be available on all platforms
static inline unsigned int rotate_left(unsigned int n, unsigned int d) {
    #ifdef __x86_64__
    unsigned int result;
    asm("roll %%cl, %0" : "=r"(result) : "0"(n), "c"(d));
    return result;
    #else
    return (n << d) | (n >> (32 - d));
    #endif
}

static inline unsigned int rotate_right(unsigned int n, unsigned int d) {
    #ifdef __x86_64__
    unsigned int result;
    asm("rorl %%cl, %0" : "=r"(result) : "0"(n), "c"(d));
    return result;
    #else
    return (n >> d) | (n << (32 - d));
    #endif
}

int main() {
    unsigned int num = 16;
    int d = 2;
    
    printf("Original: %u\n", num);
    printf("Left rotate by %d: %u\n", d, rotate_left(num, d));
    printf("Right rotate by %d: %u\n", d, rotate_right(num, d));
    
    return 0;
}
