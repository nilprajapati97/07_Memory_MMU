/* Replace Spaces with %20 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *replace_spaces(const char *str) {
    if (!str) return NULL;
    
    // Count spaces
    int space_count = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == ' ')
            space_count++;
    }
    
    // Calculate new length
    int old_len = strlen(str);
    int new_len = old_len + space_count * 2;
    
    // Allocate new string
    char *result = (char *)malloc(new_len + 1);
    if (!result) return NULL;
    
    // Replace spaces
    int j = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == ' ') {
            result[j++] = '%';
            result[j++] = '2';
            result[j++] = '0';
        } else {
            result[j++] = str[i];
        }
    }
    result[j] = '\0';
    
    return result;
}

// In-place version (requires pre-allocated space)
void replace_spaces_inplace(char *str, int true_length) {
    int space_count = 0;
    
    // Count spaces
    for (int i = 0; i < true_length; i++) {
        if (str[i] == ' ')
            space_count++;
    }
    
    // Calculate new length
    int new_length = true_length + space_count * 2;
    str[new_length] = '\0';
    
    // Replace from end
    for (int i = true_length - 1; i >= 0; i--) {
        if (str[i] == ' ') {
            str[--new_length] = '0';
            str[--new_length] = '2';
            str[--new_length] = '%';
        } else {
            str[--new_length] = str[i];
        }
    }
}

int main() {
    // Using malloc version
    const char *str1 = "Hello World from C";
    char *result = replace_spaces(str1);
    printf("Original: %s\n", str1);
    printf("Replaced: %s\n", result);
    free(result);
    
    // In-place version (need extra space)
    char str2[100] = "Mr John Smith    ";  // Extra space at end
    printf("\nOriginal: %s\n", str2);
    replace_spaces_inplace(str2, 13);  // True length without trailing spaces
    printf("Replaced: %s\n", str2);
    
    return 0;
}
