/*
 * Simple ARM64 Memory Barrier Demo (C)
 *
 * This program demonstrates the use of memory barriers in a multi-threaded context.
 * It is for educational/interview purposes and does not guarantee to trigger reordering on all systems.
 *
 * Compile: gcc -O2 -pthread -o simple_mb_demo simple_mb_demo.c
 * Run: ./simple_mb_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

volatile int X = 0, Y = 0;
volatile int r1 = 0, r2 = 0;

void *thread1(void *arg) {
    X = 1;
    __sync_synchronize(); // Full memory barrier
    r1 = Y;
    return NULL;
}

void *thread2(void *arg) {
    Y = 1;
    __sync_synchronize(); // Full memory barrier
    r2 = X;
    return NULL;
}

int main() {
    int i;
    for (i = 0; i < 1000000; ++i) {
        X = 0; Y = 0; r1 = 0; r2 = 0;
        pthread_t t1, t2;
        pthread_create(&t1, NULL, thread1, NULL);
        pthread_create(&t2, NULL, thread2, NULL);
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
        if (r1 == 0 && r2 == 0) {
            printf("Reordering observed at iteration %d! r1=%d, r2=%d\n", i, r1, r2);
            break;
        }
    }
    printf("Test finished.\n");
    return 0;
}
