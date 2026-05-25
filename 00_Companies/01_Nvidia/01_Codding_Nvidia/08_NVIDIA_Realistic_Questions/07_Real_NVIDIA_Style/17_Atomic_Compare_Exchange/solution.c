// Atomic compare-and-exchange in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>

atomic_t my_var = ATOMIC_INIT(0);

void try_update(int old, int new) {
    int ret = atomic_cmpxchg(&my_var, old, new);
    if (ret == old)
        pr_info("Updated from %d to %d\n", old, new);
    else
        pr_info("Compare-exchange failed: was %d\n", ret);
}

MODULE_LICENSE("GPL");
