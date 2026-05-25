/* Approach 2: Pattern Matching - KMP Algorithm */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void compute_lps(const char *pattern, int m, int *lps) {
    int len = 0;
    lps[0] = 0;
    int i = 1;
    
    while (i < m) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

int kmp_search(const char *text, const char *pattern) {
    int n = strlen(text);
    int m = strlen(pattern);
    int *lps = (int *)malloc(m * sizeof(int));
    int count = 0;
    
    compute_lps(pattern, m, lps);
    
    int i = 0;  // Index for text
    int j = 0;  // Index for pattern
    
    while (i < n) {
        if (pattern[j] == text[i]) {
            i++;
            j++;
        }
        
        if (j == m) {
            printf("Pattern found at index %d\n", i - j);
            count++;
            j = lps[j - 1];
        } else if (i < n && pattern[j] != text[i]) {
            if (j != 0) {
                j = lps[j - 1];
            } else {
                i++;
            }
        }
    }
    
    free(lps);
    return count;
}

int main() {
    const char *text = "AABAACAADAABAABA";
    const char *pattern = "AABA";
    
    printf("Text: %s\n", text);
    printf("Pattern: %s\n", pattern);
    printf("\nUsing KMP Algorithm:\n");
    
    int count = kmp_search(text, pattern);
    printf("Total occurrences: %d\n", count);
    
    return 0;
}
