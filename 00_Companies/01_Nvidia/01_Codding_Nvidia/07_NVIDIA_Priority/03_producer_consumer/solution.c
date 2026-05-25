// Producer-consumer using pthreads (user-space example)
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define BUF_SIZE 8

int buffer[BUF_SIZE];
int count = 0, in = 0, out = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

void *producer(void *arg) {
    for (int i = 0; i < 20; ++i) {
        pthread_mutex_lock(&mutex);
        while (count == BUF_SIZE)
            pthread_cond_wait(&not_full, &mutex);
        buffer[in] = i;
        in = (in + 1) % BUF_SIZE;
        count++;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 0; i < 20; ++i) {
        pthread_mutex_lock(&mutex);
        while (count == 0)
            pthread_cond_wait(&not_empty, &mutex);
        int item = buffer[out];
        out = (out + 1) % BUF_SIZE;
        count--;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
        printf("Consumed: %d\n", item);
    }
    return NULL;
}
