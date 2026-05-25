// Atomic reference counter in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/atomic.h>

struct my_object {
    atomic_t refcount;
};

void get_my_object(struct my_object *obj) {
    atomic_inc(&obj->refcount);
}

void put_my_object(struct my_object *obj) {
    if (atomic_dec_and_test(&obj->refcount)) {
        pr_info("Object can be freed\n");
        // kfree(obj);
    }
}

MODULE_LICENSE("GPL");
