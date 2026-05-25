/* Approach 2: Word-wise copy (optimized) */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

void *my_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    
    // Copy word-wise if aligned
    if (((uintptr_t)d | (uintptr_t)s) % sizeof(size_t) == 0) {
        while (n >= sizeof(size_t)) {
            *(size_t *)d = *(const size_t *)s;
            d += sizeof(size_t);
            s += sizeof(size_t);
            n -= sizeof(size_t);
        }
    }
    
    // Copy remaining bytes
    while (n--)
        *d++ = *s++;
    
    return dest;
}

int main() {
    char src[] = "Hello, World!";
    char dest[20];
    
    my_memcpy(dest, src, sizeof(src));
    printf("Copied: %s\n", dest);
    
    return 0;
}
