// Producer-consumer using wait queues
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#define BUF_SIZE 16
struct queue {
    int buf[BUF_SIZE];
    int head, tail, count;
    wait_queue_head_t wq;
    spinlock_t lock;
};

void queue_init(struct queue *q) {
    q->head = q->tail = q->count = 0;
    init_waitqueue_head(&q->wq);
    spin_lock_init(&q->lock);
}

void producer(struct queue *q, int val) {
    spin_lock(&q->lock);
    while (q->count == BUF_SIZE)
        spin_unlock(&q->lock);
    q->buf[q->tail] = val;
    q->tail = (q->tail + 1) % BUF_SIZE;
    q->count++;
    wake_up(&q->wq);
    spin_unlock(&q->lock);
}

int consumer(struct queue *q) {
    int val;
    wait_event(q->wq, q->count > 0);
    spin_lock(&q->lock);
    val = q->buf[q->head];
    q->head = (q->head + 1) % BUF_SIZE;
    q->count--;
    spin_unlock(&q->lock);
    return val;
}
