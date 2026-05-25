/* Approach 1: First Non-Repeating Character */
#include <stdio.h>
#include <string.h>

char first_non_repeating(const char *str) {
    if (!str || !*str) return '\0';
    
    int count[256] = {0};
    
    // Count frequency
    for (int i = 0; str[i]; i++)
        count[(unsigned char)str[i]]++;
    
    // Find first with count 1
    for (int i = 0; str[i]; i++) {
        if (count[(unsigned char)str[i]] == 1)
            return str[i];
    }
    
    return '\0';  // All characters repeat
}

int main() {
    const char *tests[] = {
        "programming",
        "aabbcc",
        "hello",
        "aabccdeff"
    };
    
    for (int i = 0; i < 4; i++) {
        char result = first_non_repeating(tests[i]);
        if (result)
            printf("\"%s\": First non-repeating = '%c'\n", tests[i], result);
        else
            printf("\"%s\": No non-repeating character\n", tests[i]);
    }
    
    return 0;
}
