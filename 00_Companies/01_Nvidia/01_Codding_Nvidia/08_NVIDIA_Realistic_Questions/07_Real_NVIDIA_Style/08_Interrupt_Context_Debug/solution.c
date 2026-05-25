// Debugging code to detect interrupt context in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

void debug_interrupt_context(void) {
    if (in_interrupt())
        pr_info("In interrupt context\n");
    else
        pr_info("Not in interrupt context\n");
}

MODULE_LICENSE("GPL");
