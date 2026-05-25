/* Approach 1: Longest Common Substring - DP */
#include <stdio.h>
#include <string.h>

int max(int a, int b) {
    return (a > b) ? a : b;
}

int longest_common_substring(const char *s1, const char *s2, char *result) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    int dp[len1 + 1][len2 + 1];
    int max_len = 0;
    int end_pos = 0;
    
    // Initialize DP table
    for (int i = 0; i <= len1; i++) {
        for (int j = 0; j <= len2; j++) {
            if (i == 0 || j == 0) {
                dp[i][j] = 0;
            } else if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
                if (dp[i][j] > max_len) {
                    max_len = dp[i][j];
                    end_pos = i - 1;
                }
            } else {
                dp[i][j] = 0;
            }
        }
    }
    
    // Extract substring
    if (result && max_len > 0) {
        strncpy(result, s1 + end_pos - max_len + 1, max_len);
        result[max_len] = '\0';
    }
    
    return max_len;
}

int main() {
    char result[100];
    
    const char *s1 = "abcdefgh";
    const char *s2 = "xycdefpq";
    
    int len = longest_common_substring(s1, s2, result);
    printf("String 1: %s\n", s1);
    printf("String 2: %s\n", s2);
    printf("Longest common substring: \"%s\" (length: %d)\n", result, len);
    
    return 0;
}
