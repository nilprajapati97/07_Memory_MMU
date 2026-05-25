// Example: Using memory barriers in Linux kernel
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

volatile int flag = 0;
int data = 0;

void writer(void) {
    data = 42;
    smp_wmb(); // Write memory barrier
    flag = 1;
}

void reader(void) {
    if (flag) {
        smp_rmb(); // Read memory barrier
        // Now safe to read data
        int val = data;
    }
}
