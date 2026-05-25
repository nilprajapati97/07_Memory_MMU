// Example of a blocking notifier chain in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(my_notifier_chain);

int my_event_notifier(struct notifier_block *nb, unsigned long action, void *data) {
    pr_info("Notifier called: action=%lu\n", action);
    return NOTIFY_OK;
}

static struct notifier_block my_nb = {
    .notifier_call = my_event_notifier,
};

static int __init notifier_example_init(void) {
    blocking_notifier_chain_register(&my_notifier_chain, &my_nb);
    blocking_notifier_call_chain(&my_notifier_chain, 1, NULL);
    return 0;
}

static void __exit notifier_example_exit(void) {
    blocking_notifier_chain_unregister(&my_notifier_chain, &my_nb);
}

module_init(notifier_example_init);
module_exit(notifier_example_exit);
MODULE_LICENSE("GPL");
