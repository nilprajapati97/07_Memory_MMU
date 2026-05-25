/* Approach 1: Byte-by-byte copy */
#include <stdio.h>
#include <stddef.h>

void *my_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    
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
