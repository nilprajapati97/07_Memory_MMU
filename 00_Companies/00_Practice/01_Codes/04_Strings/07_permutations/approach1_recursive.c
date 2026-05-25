/* Approach 1: String Permutations - Recursive */
#include <stdio.h>
#include <string.h>

void swap(char *a, char *b) {
    char temp = *a;
    *a = *b;
    *b = temp;
}

void permute(char *str, int left, int right) {
    if (left == right) {
        printf("%s\n", str);
        return;
    }
    
    for (int i = left; i <= right; i++) {
        swap(&str[left], &str[i]);
        permute(str, left + 1, right);
        swap(&str[left], &str[i]);  // Backtrack
    }
}

void print_permutations(char *str) {
    if (!str) return;
    permute(str, 0, strlen(str) - 1);
}

int main() {
    char str[] = "ABC";
    printf("Permutations of \"%s\":\n", str);
    print_permutations(str);
    
    printf("\nPermutations of \"12\":\n");
    char str2[] = "12";
    print_permutations(str2);
    
    return 0;
}
