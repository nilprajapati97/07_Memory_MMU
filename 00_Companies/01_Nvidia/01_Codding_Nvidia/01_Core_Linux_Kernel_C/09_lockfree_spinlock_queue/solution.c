// Spinlock-protected queue example
#include <linux/spinlock.h>
#define SIZE 128

struct queue {
    int data[SIZE];
    int head, tail;
    spinlock_t lock;
};

void queue_init(struct queue *q) {
    q->head = q->tail = 0;
    spin_lock_init(&q->lock);
}

void enqueue(struct queue *q, int val) {
    spin_lock(&q->lock);
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % SIZE;
    spin_unlock(&q->lock);
}

int dequeue(struct queue *q) {
    int val;
    spin_lock(&q->lock);
    val = q->data[q->head];
    q->head = (q->head + 1) % SIZE;
    spin_unlock(&q->lock);
    return val;
}
