/* memmove - handles overlapping memory */
#include <stdio.h>
#include <stddef.h>

void *my_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    
    if (d < s) {
        // Copy forward
        while (n--)
            *d++ = *s++;
    } else {
        // Copy backward
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    
    return dest;
}

int main() {
    char str[] = "Hello, World!";
    
    // Overlapping copy
    my_memmove(str + 2, str, 5);
    printf("Result: %s\n", str);
    
    return 0;
}
