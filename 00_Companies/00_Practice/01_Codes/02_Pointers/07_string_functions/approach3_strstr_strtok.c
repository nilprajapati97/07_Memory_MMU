/* Approach 3: strstr and strtok */
#include <stdio.h>
#include <string.h>

char *my_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (!*n) return (char *)haystack;
    }
    
    return NULL;
}

char *my_strtok(char *str, const char *delim) {
    static char *last = NULL;
    
    if (str) last = str;
    if (!last) return NULL;
    
    // Skip leading delimiters
    while (*last && strchr(delim, *last)) last++;
    if (!*last) return NULL;
    
    char *token = last;
    
    // Find next delimiter
    while (*last && !strchr(delim, *last)) last++;
    
    if (*last) {
        *last = '\0';
        last++;
    }
    
    return token;
}

int main() {
    // strstr test
    const char *text = "Hello, World!";
    char *found = my_strstr(text, "World");
    if (found) {
        printf("Found: %s\n", found);
    }
    
    // strtok test
    char str[] = "one,two,three,four";
    char *token = my_strtok(str, ",");
    while (token) {
        printf("Token: %s\n", token);
        token = my_strtok(NULL, ",");
    }
    
    return 0;
}
