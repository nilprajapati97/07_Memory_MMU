// Circular buffer implementation
#include <stdio.h>
#define SIZE 8

typedef struct {
    int buf[SIZE];
    int head, tail, count;
} circ_buf_t;

void cb_init(circ_buf_t *cb) {
    cb->head = cb->tail = cb->count = 0;
}

int cb_push(circ_buf_t *cb, int val) {
    if (cb->count == SIZE) return -1; // full
    cb->buf[cb->head] = val;
    cb->head = (cb->head + 1) % SIZE;
    cb->count++;
    return 0;
}

int cb_pop(circ_buf_t *cb, int *val) {
    if (cb->count == 0) return -1; // empty
    *val = cb->buf[cb->tail];
    cb->tail = (cb->tail + 1) % SIZE;
    cb->count--;
    return 0;
}
