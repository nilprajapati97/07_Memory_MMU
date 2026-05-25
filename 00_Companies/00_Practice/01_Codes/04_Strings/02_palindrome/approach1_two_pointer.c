/* Approach 1: Check Palindrome - Two Pointer */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

bool is_palindrome(const char *str) {
    if (!str) return false;
    
    int left = 0;
    int right = strlen(str) - 1;
    
    while (left < right) {
        // Skip non-alphanumeric characters
        while (left < right && !isalnum(str[left]))
            left++;
        while (left < right && !isalnum(str[right]))
            right--;
        
        // Compare (case-insensitive)
        if (tolower(str[left]) != tolower(str[right]))
            return false;
        
        left++;
        right--;
    }
    
    return true;
}

int main() {
    const char *tests[] = {
        "racecar",
        "A man, a plan, a canal: Panama",
        "hello",
        "Madam",
        "12321",
        "12345"
    };
    
    for (int i = 0; i < 6; i++) {
        printf("\"%s\" is %s palindrome\n", 
               tests[i], 
               is_palindrome(tests[i]) ? "a" : "not a");
    }
    
    return 0;
}
