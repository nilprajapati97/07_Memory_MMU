/* Approach 2: strcmp and strcat */
#include <stdio.h>

int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *my_strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}

int main() {
    char s1[] = "Hello";
    char s2[] = "Hello";
    char s3[] = "World";
    
    printf("strcmp(\"%s\", \"%s\") = %d\n", s1, s2, my_strcmp(s1, s2));
    printf("strcmp(\"%s\", \"%s\") = %d\n", s1, s3, my_strcmp(s1, s3));
    
    char dest[50] = "Hello, ";
    my_strcat(dest, "World!");
    printf("strcat result: %s\n", dest);
    
    return 0;
}
