/* Approach 1: Remove Duplicates - In-place */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

void remove_duplicates(char *str) {
    if (!str || !*str) return;
    
    bool seen[256] = {false};
    int write_idx = 0;
    
    for (int read_idx = 0; str[read_idx]; read_idx++) {
        unsigned char c = str[read_idx];
        
        if (!seen[c]) {
            seen[c] = true;
            str[write_idx++] = str[read_idx];
        }
    }
    
    str[write_idx] = '\0';
}

int main() {
    char str1[] = "programming";
    printf("Original: %s\n", str1);
    remove_duplicates(str1);
    printf("After removing duplicates: %s\n", str1);
    
    char str2[] = "aabbccddee";
    printf("\nOriginal: %s\n", str2);
    remove_duplicates(str2);
    printf("After removing duplicates: %s\n", str2);
    
    return 0;
}
