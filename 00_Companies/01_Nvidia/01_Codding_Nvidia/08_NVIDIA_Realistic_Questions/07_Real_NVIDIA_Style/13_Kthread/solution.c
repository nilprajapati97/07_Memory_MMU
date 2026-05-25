// Kernel thread (kthread) example in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct task_struct *my_kthread;
static int kthread_should_run = 1;

int kthread_fn(void *data) {
    while (!kthread_should_stop()) {
        pr_info("kthread running\n");
        msleep(1000);
    }
    return 0;
}

static int __init kthread_example_init(void) {
    my_kthread = kthread_run(kthread_fn, NULL, "my_kthread");
    return PTR_ERR_OR_ZERO(my_kthread);
}

static void __exit kthread_example_exit(void) {
    if (my_kthread)
        kthread_stop(my_kthread);
}

module_init(kthread_example_init);
module_exit(kthread_example_exit);
MODULE_LICENSE("GPL");
