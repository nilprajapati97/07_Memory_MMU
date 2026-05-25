/* Approach 4: strncpy (safe version) */
#include <stdio.h>

char *my_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    
    for (; i < n; i++)
        dest[i] = '\0';
    
    return dest;
}

int main() {
    char src[] = "Hello, World!";
    char dest[20];
    
    my_strncpy(dest, src, 5);
    dest[5] = '\0';  // Ensure null termination
    printf("strncpy(5): %s\n", dest);
    
    my_strncpy(dest, src, sizeof(dest) - 1);
    dest[sizeof(dest) - 1] = '\0';
    printf("strncpy(full): %s\n", dest);
    
    return 0;
}
