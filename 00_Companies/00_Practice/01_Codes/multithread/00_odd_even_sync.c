/*
================================================================================
        MULTITHREADING: ODD-EVEN PRINTER WITH SYNCHRONIZATION
================================================================================

PURPOSE:
This program demonstrates core multithreading concepts:
1. Thread creation and management
2. Mutex locking/unlocking
3. Condition variables for synchronization
4. Thread joining
5. Race condition prevention
6. Atomic operations and memory barriers

PROGRAM FLOW:
- Main thread creates 2 worker threads
- Thread 1: Prints odd numbers (1, 3, 5, ..., 99)
- Thread 2: Prints even numbers (2, 4, 6, ..., 100)
- Threads synchronize using mutex + condition variable
- Both threads print numbers sequentially (1, 2, 3, 4, ...)
- Main thread waits for both worker threads to complete
- Program terminates cleanly

COMPILATION:
gcc -o odd_even_sync odd_even_sync.c -lpthread -Wall

EXECUTION:
./odd_even_sync

EXPECTED OUTPUT:
[ODD] Printing number: 1
[EVEN] Printing number: 2
[ODD] Printing number: 3
[EVEN] Printing number: 4
...
[ODD] Printing number: 99
[EVEN] Printing number: 100

================================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// ============================================================================
// GLOBAL VARIABLES - SHARED BETWEEN THREADS
// ============================================================================

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int counter = 1;
int turn = 0;  // 0 = Odd thread's turn, 1 = Even thread's turn

#define LIMIT 100

// ============================================================================
// THREAD FUNCTION 1: ODD NUMBER PRINTER
// ============================================================================

void* thread_print_odd(void* arg) {
    (void)arg;
    
    printf("Odd thread started (ID: %lu)\n", pthread_self());
    
    pthread_mutex_lock(&mutex);
    
    while (counter <= LIMIT) {
        if (turn == 0 && counter % 2 == 1) {
            printf("[ODD] Printing number: %d\n", counter);
            fflush(stdout);
            
            counter++;
            turn = 1;
            
            pthread_cond_signal(&condition);
        } 
        else 
        {
            pthread_cond_wait(&condition, &mutex);
        }
    }
    
    pthread_mutex_unlock(&mutex);
    
    printf("Odd thread finished (ID: %lu)\n", pthread_self());
    pthread_exit(NULL);
}

// ============================================================================
// THREAD FUNCTION 2: EVEN NUMBER PRINTER
// ============================================================================

void* thread_print_even(void* arg) {
    (void)arg;
    
    printf("Even thread started (ID: %lu)\n", pthread_self());
    
    pthread_mutex_lock(&mutex);
    
    while (counter <= LIMIT) {
        if (turn == 1 && counter % 2 == 0) {
            printf("[EVEN] Printing number: %d\n", counter);
            fflush(stdout);
            
            counter++;
            turn = 0;
            
            pthread_cond_signal(&condition);
        } 
        else {
            pthread_cond_wait(&condition, &mutex);
        }
    }
    
    pthread_mutex_unlock(&mutex);
    
    printf("Even thread finished (ID: %lu)\n", pthread_self());
    pthread_exit(NULL);
}

// ============================================================================
// MAIN THREAD
// ============================================================================

int main() {
    printf("================================================================================\n");
    printf("         MULTITHREADING: ODD-EVEN SYNCHRONIZATION DEMO\n");
    printf("================================================================================\n\n");
    
    printf("Main thread started (ID: %lu)\n", pthread_self());
    printf("Creating worker threads...\n\n");
    
    pthread_t thread_odd_id, thread_even_id;
    
    int ret1 = pthread_create(&thread_odd_id, NULL, thread_print_odd, NULL);
    if (ret1 != 0) {
        printf("Error creating odd thread: %d\n", ret1);
        return 1;
    }
    
    int ret2 = pthread_create(&thread_even_id, NULL, thread_print_even, NULL);
    if (ret2 != 0) {
        printf("Error creating even thread: %d\n", ret2);
        return 1;
    }
    
    printf("Both threads created successfully!\n");
    printf("Waiting for threads to complete...\n\n");
    
    int join_ret1 = pthread_join(thread_odd_id, NULL);
    if (join_ret1 != 0) {
        printf("Error joining odd thread: %d\n", join_ret1);
        return 1;
    }
    
    int join_ret2 = pthread_join(thread_even_id, NULL);
    if (join_ret2 != 0) {
        printf("Error joining even thread: %d\n", join_ret2);
        return 1;
    }
    
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condition);
    
    printf("\n================================================================================\n");
    printf("All threads completed successfully!\n");
    printf("Final counter value: %d (should be %d)\n", counter, LIMIT + 1);
    printf("Main thread exiting gracefully.\n");
    printf("================================================================================\n");
    
    return 0;
}
