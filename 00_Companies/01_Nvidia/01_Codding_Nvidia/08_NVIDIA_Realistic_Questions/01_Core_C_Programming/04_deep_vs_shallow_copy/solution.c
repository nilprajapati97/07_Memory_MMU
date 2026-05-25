// Deep copy vs shallow copy example
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Data {
    int value;
    char *str;
};

// Shallow copy
void shallow_copy(struct Data *src, struct Data *dst) {
    *dst = *src;
}

// Deep copy
void deep_copy(struct Data *src, struct Data *dst) {
    dst->value = src->value;
    dst->str = malloc(strlen(src->str) + 1);
    strcpy(dst->str, src->str);
}
