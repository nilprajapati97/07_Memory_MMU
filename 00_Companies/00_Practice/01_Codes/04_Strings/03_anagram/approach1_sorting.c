/* Approach 1: Check Anagram - Sorting */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

int compare(const void *a, const void *b) {
    return *(char *)a - *(char *)b;
}

bool is_anagram(const char *s1, const char *s2) {
    if (!s1 || !s2) return false;
    
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    if (len1 != len2) return false;
    
    // Copy and convert to lowercase
    char *str1 = malloc(len1 + 1);
    char *str2 = malloc(len2 + 1);
    
    for (int i = 0; i < len1; i++) {
        str1[i] = tolower(s1[i]);
        str2[i] = tolower(s2[i]);
    }
    str1[len1] = '\0';
    str2[len2] = '\0';
    
    // Sort both strings
    qsort(str1, len1, sizeof(char), compare);
    qsort(str2, len2, sizeof(char), compare);
    
    // Compare
    bool result = strcmp(str1, str2) == 0;
    
    free(str1);
    free(str2);
    
    return result;
}

int main() {
    printf("\"listen\" and \"silent\": %s\n", 
           is_anagram("listen", "silent") ? "Anagram" : "Not anagram");
    
    printf("\"hello\" and \"world\": %s\n", 
           is_anagram("hello", "world") ? "Anagram" : "Not anagram");
    
    printf("\"Dormitory\" and \"Dirty room\": %s\n", 
           is_anagram("Dormitory", "Dirtyroom") ? "Anagram" : "Not anagram");
    
    return 0;
}
