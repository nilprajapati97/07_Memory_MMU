#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_NUMBER 100

// Shared data with synchronization primitives
typedef struct {
    int counter;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_odd_turn;
} SharedData;

SharedData shared_data;

// Odd number thread
void* print_odd(void* arg) {
    while (1) {
        pthread_mutex_lock(&shared_data.mutex);
        
        // Wait for odd's turn
        while (!shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
        }
        
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Print odd number
        printf("ODD Thread:  %d\n", shared_data.counter);
        
        shared_data.counter++;
        shared_data.is_odd_turn = false;
        
        pthread_cond_signal(&shared_data.cond);
        pthread_mutex_unlock(&shared_data.mutex);
        
        usleep(5000); // Small delay
    }
    
    return NULL;
}

// Even number thread
void* print_even(void* arg) {
    while (1) {
        pthread_mutex_lock(&shared_data.mutex);
        
        // Wait for even's turn
        while (shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
        }
        
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Print even number
        printf("EVEN Thread: %d\n", shared_data.counter);
        
        shared_data.counter++;
        shared_data.is_odd_turn = true;
        
        pthread_cond_signal(&shared_data.cond);
        pthread_mutex_unlock(&shared_data.mutex);
        
        usleep(5000); // Small delay
    }
    
    return NULL;
}

int main() {
    pthread_t odd_thread, even_thread;
    
    printf("=== Multithreading: Odd/Even Number Printing ===\n\n");
    
    // Initialize
    shared_data.counter = 1;
    shared_data.is_odd_turn = true;
    pthread_mutex_init(&shared_data.mutex, NULL);
    pthread_cond_init(&shared_data.cond, NULL);
    
    // Create threads
    pthread_create(&odd_thread, NULL, print_odd, NULL);
    pthread_create(&even_thread, NULL, print_even, NULL);
    
    // Wait for threads to complete
    pthread_join(odd_thread, NULL);
    pthread_join(even_thread, NULL);
    
    // Cleanup
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond);
    
    printf("\n=== All threads completed and joined to main ===\n");
    printf("Main thread exiting...\n\n");
    
    return 0;
}
