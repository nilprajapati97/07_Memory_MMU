// Usage of container_of macro in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>

struct my_struct {
    int data;
    struct list_head list;
};

void example(struct list_head *entry) {
    struct my_struct *obj = container_of(entry, struct my_struct, list);
    pr_info("data = %d\n", obj->data);
}

MODULE_LICENSE("GPL");
