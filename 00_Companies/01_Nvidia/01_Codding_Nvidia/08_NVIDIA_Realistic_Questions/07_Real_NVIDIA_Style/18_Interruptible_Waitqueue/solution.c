// Interruptible waitqueue example in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/kthread.h>

static DECLARE_WAIT_QUEUE_HEAD(my_wq);
static int condition = 0;
static struct task_struct *my_kthread;

int kthread_fn(void *data) {
    msleep(1000);
    condition = 1;
    wake_up_interruptible(&my_wq);
    return 0;
}

static int __init waitqueue_example_init(void) {
    my_kthread = kthread_run(kthread_fn, NULL, "my_kthread");
    wait_event_interruptible(my_wq, condition);
    return 0;
}

static void __exit waitqueue_example_exit(void) {
    if (my_kthread)
        kthread_stop(my_kthread);
}

module_init(waitqueue_example_init);
module_exit(waitqueue_example_exit);
MODULE_LICENSE("GPL");
