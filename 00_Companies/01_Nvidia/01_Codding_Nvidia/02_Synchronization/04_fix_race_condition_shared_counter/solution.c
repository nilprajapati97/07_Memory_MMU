// Fix a race condition in shared counter code
#include <linux/spinlock.h>

struct counter {
    int value;
    spinlock_t lock;
};

void counter_init(struct counter *c) {
    c->value = 0;
    spin_lock_init(&c->lock);
}

void increment(struct counter *c) {
    spin_lock(&c->lock);
    c->value++;
    spin_unlock(&c->lock);
}
