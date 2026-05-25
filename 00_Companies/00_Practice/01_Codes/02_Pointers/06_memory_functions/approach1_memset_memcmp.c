/* memset and memcmp implementations */
#include <stdio.h>
#include <stddef.h>

void *my_memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

int my_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    
    while (n--) {
        if (*p1 != *p2)
            return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

int main() {
    char buf[20];
    
    my_memset(buf, 'A', 10);
    buf[10] = '\0';
    printf("memset: %s\n", buf);
    
    char s1[] = "Hello";
    char s2[] = "Hello";
    printf("memcmp: %d\n", my_memcmp(s1, s2, 5));
    
    return 0;
}
