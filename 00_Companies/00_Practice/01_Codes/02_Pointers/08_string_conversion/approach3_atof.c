/* Approach 3: atof (ASCII to Float) */
#include <stdio.h>
#include <ctype.h>

double my_atof(const char *str) {
    double result = 0.0;
    double fraction = 0.0;
    int sign = 1;
    int divisor = 1;
    int in_fraction = 0;
    
    // Skip whitespace
    while (isspace(*str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Process digits
    while (*str) {
        if (isdigit(*str)) {
            if (in_fraction) {
                fraction = fraction * 10 + (*str - '0');
                divisor *= 10;
            } else {
                result = result * 10 + (*str - '0');
            }
        } else if (*str == '.' && !in_fraction) {
            in_fraction = 1;
        } else {
            break;
        }
        str++;
    }
    
    return sign * (result + fraction / divisor);
}

int main() {
    printf("atof(\"123.45\") = %.2f\n", my_atof("123.45"));
    printf("atof(\"-67.89\") = %.2f\n", my_atof("-67.89"));
    printf("atof(\"3.14159\") = %.5f\n", my_atof("3.14159"));
    printf("atof(\"  42.0\") = %.1f\n", my_atof("  42.0"));
    
    return 0;
}
