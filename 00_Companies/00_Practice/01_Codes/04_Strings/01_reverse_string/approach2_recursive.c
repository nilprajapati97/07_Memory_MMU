/* Approach 2: Reverse String - Recursive */
#include <stdio.h>
#include <string.h>

void reverse_recursive_helper(char *str, int left, int right) {
    if (left >= right) return;
    
    // Swap
    char temp = str[left];
    str[left] = str[right];
    str[right] = temp;
    
    // Recurse
    reverse_recursive_helper(str, left + 1, right - 1);
}

void reverse_recursive(char *str) {
    if (!str) return;
    reverse_recursive_helper(str, 0, strlen(str) - 1);
}

int main() {
    char str[] = "Recursion";
    printf("Original: %s\n", str);
    reverse_recursive(str);
    printf("Reversed: %s\n", str);
    
    return 0;
}
