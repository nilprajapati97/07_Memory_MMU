// Single-producer single-consumer ring buffer
#include <stdatomic.h>
#define SIZE 8

typedef struct {
    int buf[SIZE];
    atomic_int head, tail;
} spsc_ring_t;

void spsc_init(spsc_ring_t *rb) {
    atomic_store(&rb->head, 0);
    atomic_store(&rb->tail, 0);
}

int spsc_push(spsc_ring_t *rb, int val) {
    int head = atomic_load(&rb->head);
    int next = (head + 1) % SIZE;
    if (next == atomic_load(&rb->tail)) return -1; // full
    rb->buf[head] = val;
    atomic_store(&rb->head, next);
    return 0;
}

int spsc_pop(spsc_ring_t *rb, int *val) {
    int tail = atomic_load(&rb->tail);
    if (tail == atomic_load(&rb->head)) return -1; // empty
    *val = rb->buf[tail];
    atomic_store(&rb->tail, (tail + 1) % SIZE);
    return 0;
}
