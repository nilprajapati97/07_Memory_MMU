/* Approach 2: Union (Clean) */
#include <stdio.h>

int is_little_endian() {
    union {
        unsigned int i;
        char c[4];
    } test = {0x01020304};
    
    return test.c[0] == 4;
}

int main() {
    union {
        unsigned int i;
        char c[4];
    } test = {0x01020304};
    
    printf("Bytes: %02X %02X %02X %02X\n", 
           test.c[0], test.c[1], test.c[2], test.c[3]);
    
    if (is_little_endian())
        printf("System is Little Endian\n");
    else
        printf("System is Big Endian\n");
    
    return 0;
}
