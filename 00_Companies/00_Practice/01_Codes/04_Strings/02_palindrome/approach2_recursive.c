/* Approach 2: Check Palindrome - Recursive */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

bool is_palindrome_helper(const char *str, int left, int right) {
    if (left >= right)
        return true;
    
    // Skip non-alphanumeric
    while (left < right && !isalnum(str[left]))
        left++;
    while (left < right && !isalnum(str[right]))
        right--;
    
    // Compare
    if (tolower(str[left]) != tolower(str[right]))
        return false;
    
    return is_palindrome_helper(str, left + 1, right - 1);
}

bool is_palindrome(const char *str) {
    if (!str) return false;
    return is_palindrome_helper(str, 0, strlen(str) - 1);
}

int main() {
    const char *tests[] = {
        "racecar",
        "A man, a plan, a canal: Panama",
        "hello"
    };
    
    for (int i = 0; i < 3; i++) {
        printf("\"%s\" is %s palindrome\n", 
               tests[i], 
               is_palindrome(tests[i]) ? "a" : "not a");
    }
    
    return 0;
}
