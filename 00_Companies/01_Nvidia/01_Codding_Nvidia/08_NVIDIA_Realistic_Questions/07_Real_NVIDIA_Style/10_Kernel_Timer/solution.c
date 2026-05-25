// Kernel timer example in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>

static struct timer_list my_timer;

void timer_callback(struct timer_list *t) {
    pr_info("Timer callback executed\n");
}

static int __init timer_example_init(void) {
    timer_setup(&my_timer, timer_callback, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(1000)); // 1 second
    return 0;
}

static void __exit timer_example_exit(void) {
    del_timer_sync(&my_timer);
}

module_init(timer_example_init);
module_exit(timer_example_exit);
MODULE_LICENSE("GPL");
