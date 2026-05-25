/* Approach 1: Pattern Matching - Naive */
#include <stdio.h>
#include <string.h>

int naive_search(const char *text, const char *pattern) {
    int n = strlen(text);
    int m = strlen(pattern);
    int count = 0;
    
    for (int i = 0; i <= n - m; i++) {
        int j;
        for (j = 0; j < m; j++) {
            if (text[i + j] != pattern[j])
                break;
        }
        
        if (j == m) {
            printf("Pattern found at index %d\n", i);
            count++;
        }
    }
    
    return count;
}

int main() {
    const char *text = "AABAACAADAABAABA";
    const char *pattern = "AABA";
    
    printf("Text: %s\n", text);
    printf("Pattern: %s\n", pattern);
    
    int count = naive_search(text, pattern);
    printf("Total occurrences: %d\n", count);
    
    return 0;
}
