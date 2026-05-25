/* Approach 3: Compile-time Check (Macro) */
#include <stdio.h>

#define IS_LITTLE_ENDIAN() ({ \
    unsigned int x = 1; \
    *(char *)&x; \
})

int main() {
    if (IS_LITTLE_ENDIAN())
        printf("System is Little Endian\n");
    else
        printf("System is Big Endian\n");
    
    // Alternative: Check at compile time
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        printf("Compile-time: Little Endian\n");
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        printf("Compile-time: Big Endian\n");
    #endif
    
    return 0;
}
