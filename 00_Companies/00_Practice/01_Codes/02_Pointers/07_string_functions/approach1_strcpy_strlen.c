/* Approach 1: strcpy and strlen */
#include <stdio.h>

char *my_strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

size_t my_strlen(const char *str) {
    const char *s = str;
    while (*s) s++;
    return s - str;
}

int main() {
    char src[] = "Hello, World!";
    char dest[50];
    
    my_strcpy(dest, src);
    printf("Copied: %s\n", dest);
    printf("Length: %zu\n", my_strlen(dest));
    
    return 0;
}
