/* Count Vowels, Consonants, Words, Lines */
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    int vowels;
    int consonants;
    int words;
    int lines;
    int digits;
    int spaces;
} StringStats;

bool is_vowel(char c) {
    c = tolower(c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

StringStats count_all(const char *str) {
    StringStats stats = {0, 0, 0, 0, 0, 0};
    bool in_word = false;
    
    if (!str) return stats;
    
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        
        // Count lines
        if (c == '\n')
            stats.lines++;
        
        // Count vowels and consonants
        if (isalpha(c)) {
            if (is_vowel(c))
                stats.vowels++;
            else
                stats.consonants++;
            
            if (!in_word) {
                stats.words++;
                in_word = true;
            }
        } else {
            in_word = false;
        }
        
        // Count digits
        if (isdigit(c))
            stats.digits++;
        
        // Count spaces
        if (isspace(c))
            stats.spaces++;
    }
    
    // If string doesn't end with newline, count as one line
    if (str[0] != '\0')
        stats.lines++;
    
    return stats;
}

void print_stats(const char *str) {
    StringStats stats = count_all(str);
    
    printf("String: \"%s\"\n", str);
    printf("Vowels: %d\n", stats.vowels);
    printf("Consonants: %d\n", stats.consonants);
    printf("Words: %d\n", stats.words);
    printf("Lines: %d\n", stats.lines);
    printf("Digits: %d\n", stats.digits);
    printf("Spaces: %d\n", stats.spaces);
}

int main() {
    const char *text = "Hello World!\nThis is line 2.\nNumbers: 123";
    print_stats(text);
    
    return 0;
}
