// Detect integer overflow in kernel C
#include <limits.h>
#include <stdbool.h>

bool add_overflow(int a, int b, int *result) {
    if (((b > 0) && (a > INT_MAX - b)) ||
        ((b < 0) && (a < INT_MIN - b))) {
        return true; // overflow
    }
    *result = a + b;
    return false;
}
