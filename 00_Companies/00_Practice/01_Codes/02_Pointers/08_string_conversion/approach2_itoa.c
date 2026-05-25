/* Approach 2: itoa (Integer to ASCII) */
#include <stdio.h>
#include <string.h>

void reverse(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

char *my_itoa(int num, char *str, int base) {
    int i = 0;
    int is_negative = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
    
    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    if (is_negative)
        str[i++] = '-';
    
    str[i] = '\0';
    reverse(str);
    
    return str;
}

int main() {
    char buffer[50];
    
    printf("itoa(123, 10) = %s\n", my_itoa(123, buffer, 10));
    printf("itoa(-456, 10) = %s\n", my_itoa(-456, buffer, 10));
    printf("itoa(255, 16) = %s\n", my_itoa(255, buffer, 16));
    printf("itoa(8, 2) = %s\n", my_itoa(8, buffer, 2));
    
    return 0;
}
