// Use-after-free bug example
#include <stdlib.h>
#include <stdio.h>

void use_after_free() {
    int *ptr = malloc(sizeof(int));
    *ptr = 42;
    free(ptr);
    printf("UAF: %d\n", *ptr); // Undefined behavior
}
