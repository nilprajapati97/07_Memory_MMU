/* Approach 1: Pointer Cast (Most Common) */
#include <stdio.h>

int is_little_endian() {
    unsigned int x = 1;
    return *(char *)&x == 1;
}

int main() {
    if (is_little_endian())
        printf("System is Little Endian\n");
    else
        printf("System is Big Endian\n");
    
    return 0;
}
