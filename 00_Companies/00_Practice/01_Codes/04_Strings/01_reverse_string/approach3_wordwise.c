/* Approach 3: Reverse String - Word-wise */
#include <stdio.h>
#include <string.h>

void reverse_word(char *start, char *end) {
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

void reverse_words(char *str) {
    if (!str) return;
    
    int len = strlen(str);
    
    // Reverse entire string
    reverse_word(str, str + len - 1);
    
    // Reverse each word
    char *word_start = str;
    char *temp = str;
    
    while (*temp) {
        temp++;
        if (*temp == '\0' || *temp == ' ') {
            reverse_word(word_start, temp - 1);
            word_start = temp + 1;
        }
    }
}

int main() {
    char str[] = "Hello World from C";
    printf("Original: %s\n", str);
    reverse_words(str);
    printf("Word-wise reversed: %s\n", str);
    
    return 0;
}
