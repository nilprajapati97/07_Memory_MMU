/* Approach 1: Reverse String - In-place */
#include <stdio.h>
#include <string.h>

void reverse_inplace(char *str) {
    if (!str) return;
    
    int left = 0;
    int right = strlen(str) - 1;
    
    while (left < right) {
        // Swap characters
        char temp = str[left];
        str[left] = str[right];
        str[right] = temp;
        
        left++;
        right--;
    }
}

int main() {
    char str1[] = "Hello, World!";
    printf("Original: %s\n", str1);
    reverse_inplace(str1);
    printf("Reversed: %s\n", str1);
    
    char str2[] = "12345";
    printf("\nOriginal: %s\n", str2);
    reverse_inplace(str2);
    printf("Reversed: %s\n", str2);
    
    return 0;
}
