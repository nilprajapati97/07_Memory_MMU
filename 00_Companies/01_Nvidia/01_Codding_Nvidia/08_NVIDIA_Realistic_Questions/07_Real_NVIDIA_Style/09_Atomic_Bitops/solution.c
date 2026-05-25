// Atomic bit operations in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>

unsigned long flags = 0;

void set_flag(int bit) {
    set_bit(bit, &flags);
}

void clear_flag(int bit) {
    clear_bit(bit, &flags);
}

int test_flag(int bit) {
    return test_bit(bit, &flags);
}

MODULE_LICENSE("GPL");
