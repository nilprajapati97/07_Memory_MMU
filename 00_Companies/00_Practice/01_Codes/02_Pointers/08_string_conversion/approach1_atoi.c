/* Approach 1: atoi (ASCII to Integer) */
#include <stdio.h>
#include <ctype.h>

int my_atoi(const char *str) {
    int result = 0;
    int sign = 1;
    
    // Skip whitespace
    while (isspace(*str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

int main() {
    printf("atoi(\"123\") = %d\n", my_atoi("123"));
    printf("atoi(\"-456\") = %d\n", my_atoi("-456"));
    printf("atoi(\"  789\") = %d\n", my_atoi("  789"));
    printf("atoi(\"+42\") = %d\n", my_atoi("+42"));
    
    return 0;
}
