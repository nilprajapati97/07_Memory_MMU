// Completion synchronization primitive in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/kthread.h>

static DECLARE_COMPLETION(my_completion);
static struct task_struct *my_kthread;

int kthread_fn(void *data) {
    pr_info("Thread sleeping\n");
    msleep(1000);
    complete(&my_completion);
    return 0;
}

static int __init completion_example_init(void) {
    my_kthread = kthread_run(kthread_fn, NULL, "my_kthread");
    wait_for_completion(&my_completion);
    return 0;
}

static void __exit completion_example_exit(void) {
    if (my_kthread)
        kthread_stop(my_kthread);
}

module_init(completion_example_init);
module_exit(completion_example_exit);
MODULE_LICENSE("GPL");
