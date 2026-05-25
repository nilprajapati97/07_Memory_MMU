/* Approach 2: Check Anagram - Frequency Count */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

bool is_anagram(const char *s1, const char *s2) {
    if (!s1 || !s2) return false;
    
    int count[26] = {0};
    
    // Count characters in first string
    for (int i = 0; s1[i]; i++) {
        if (isalpha(s1[i]))
            count[tolower(s1[i]) - 'a']++;
    }
    
    // Subtract characters from second string
    for (int i = 0; s2[i]; i++) {
        if (isalpha(s2[i]))
            count[tolower(s2[i]) - 'a']--;
    }
    
    // Check if all counts are zero
    for (int i = 0; i < 26; i++) {
        if (count[i] != 0)
            return false;
    }
    
    return true;
}

int main() {
    printf("\"listen\" and \"silent\": %s\n", 
           is_anagram("listen", "silent") ? "Anagram" : "Not anagram");
    
    printf("\"The Eyes\" and \"They See\": %s\n", 
           is_anagram("The Eyes", "They See") ? "Anagram" : "Not anagram");
    
    printf("\"hello\" and \"world\": %s\n", 
           is_anagram("hello", "world") ? "Anagram" : "Not anagram");
    
    return 0;
}
