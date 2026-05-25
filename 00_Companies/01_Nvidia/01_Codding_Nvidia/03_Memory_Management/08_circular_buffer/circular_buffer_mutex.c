// Thread-safe circular buffer using mutexes
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define BUFFER_SIZE 8

typedef struct {
    int buffer[BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} CircularBuffer;

void cb_init(CircularBuffer *cb) {
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
    pthread_mutex_init(&cb->lock, NULL);
    pthread_cond_init(&cb->not_full, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
}

void cb_destroy(CircularBuffer *cb) {
    pthread_mutex_destroy(&cb->lock);
    pthread_cond_destroy(&cb->not_full);
    pthread_cond_destroy(&cb->not_empty);
}

// Producer: add item
void cb_push(CircularBuffer *cb, int item) {
    pthread_mutex_lock(&cb->lock);
    while (cb->count == BUFFER_SIZE) {
        pthread_cond_wait(&cb->not_full, &cb->lock);
    }
    cb->buffer[cb->head] = item;
    cb->head = (cb->head + 1) % BUFFER_SIZE;
    cb->count++;
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
}

// Consumer: remove item
int cb_pop(CircularBuffer *cb) {
    pthread_mutex_lock(&cb->lock);
    while (cb->count == 0) {
        pthread_cond_wait(&cb->not_empty, &cb->lock);
    }
    int item = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % BUFFER_SIZE;
    cb->count--;
    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->lock);
    return item;
}

// Demo: single producer/consumer
void *producer(void *arg) {
    CircularBuffer *cb = (CircularBuffer*)arg;
    for (int i = 1; i <= 20; ++i) {
        cb_push(cb, i);
        printf("Produced: %d\n", i);
    }
    return NULL;
}

void *consumer(void *arg) {
    CircularBuffer *cb = (CircularBuffer*)arg;
    for (int i = 1; i <= 20; ++i) {
        int val = cb_pop(cb);
        printf("Consumed: %d\n", val);
    }
    return NULL;
}

int main() {
    CircularBuffer cb;
    cb_init(&cb);
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, &cb);
    pthread_create(&cons, NULL, consumer, &cb);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    cb_destroy(&cb);
    return 0;
}
