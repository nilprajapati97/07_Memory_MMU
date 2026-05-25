// Simple RCU-protected linked list example
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/module.h>

struct my_node {
    int data;
    struct list_head list;
};

struct list_head my_list;

void add_node(int val) {
    struct my_node *node = kmalloc(sizeof(*node), GFP_KERNEL);
    node->data = val;
    INIT_LIST_HEAD(&node->list);
    list_add_rcu(&node->list, &my_list);
}

void remove_node(struct my_node *node) {
    list_del_rcu(&node->list);
    synchronize_rcu();
    kfree(node);
}

int sum_list(void) {
    int sum = 0;
    struct my_node *node;
    rcu_read_lock();
    list_for_each_entry_rcu(node, &my_list, list) {
        sum += node->data;
    }
    rcu_read_unlock();
    return sum;
}

// Module init/exit for demonstration (not for production)
static int __init rcu_demo_init(void) {
    INIT_LIST_HEAD(&my_list);
    add_node(1);
    add_node(2);
    add_node(3);
    return 0;
}

static void __exit rcu_demo_exit(void) {
    struct my_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &my_list, list) {
        remove_node(node);
    }
}

module_init(rcu_demo_init);
module_exit(rcu_demo_exit);
MODULE_LICENSE("GPL");
