// Deferred work using workqueues in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

static struct work_struct my_work;

void work_handler(struct work_struct *work) {
    pr_info("Workqueue handler executed\n");
}

static int __init workqueue_example_init(void) {
    INIT_WORK(&my_work, work_handler);
    schedule_work(&my_work);
    return 0;
}

static void __exit workqueue_example_exit(void) {
    flush_scheduled_work();
}

module_init(workqueue_example_init);
module_exit(workqueue_example_exit);
MODULE_LICENSE("GPL");
