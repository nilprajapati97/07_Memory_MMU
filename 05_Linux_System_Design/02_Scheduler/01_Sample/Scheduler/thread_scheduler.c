#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_NUMBER 100

// Shared data structure
typedef struct {
    int counter;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_odd_turn;
} SharedData;

// Thread statistics
typedef struct {
    int thread_id;
    char name[20];
    int numbers_printed;
    int context_switches;
    int wait_count;
} ThreadStats;

SharedData shared_data;
ThreadStats odd_stats = {1, "ODD_THREAD", 0, 0, 0};
ThreadStats even_stats = {2, "EVEN_THREAD", 0, 0, 0};

// Simulate scheduler context switch
void simulate_context_switch(ThreadStats* stats) {
    stats->context_switches++;
    printf("        [SCHEDULER] Context switch for %s (Total: %d)\n", 
           stats->name, stats->context_switches);
}

// Thread function to print odd numbers
void* print_odd(void* arg) {
    printf("\n[THREAD CREATED] %s (TID: %ld) - State: READY\n", 
           odd_stats.name, pthread_self());
    
    while (1) {
        // Acquire mutex lock
        pthread_mutex_lock(&shared_data.mutex);
        printf("    [LOCK ACQUIRED] %s acquired mutex\n", odd_stats.name);
        
        // Wait until it's odd's turn
        while (!shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            printf("    [WAITING] %s waiting for condition (counter=%d)\n", 
                   odd_stats.name, shared_data.counter);
            odd_stats.wait_count++;
            
            // Thread goes to WAITING state
            printf("    [STATE CHANGE] %s: RUNNING -> WAITING\n", odd_stats.name);
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
            printf("    [STATE CHANGE] %s: WAITING -> READY -> RUNNING\n", odd_stats.name);
            
            simulate_context_switch(&odd_stats);
        }
        
        // Check if we've reached the limit
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Critical section: Print odd number
        printf("\n>>> [%s] Printing: %d\n", odd_stats.name, shared_data.counter);
        odd_stats.numbers_printed++;
        
        // Update shared state
        shared_data.counter++;
        shared_data.is_odd_turn = false;
        
        // Signal even thread
        printf("    [SIGNAL] %s signaling EVEN_THREAD\n", odd_stats.name);
        pthread_cond_signal(&shared_data.cond);
        
        // Release mutex
        printf("    [LOCK RELEASED] %s released mutex\n\n", odd_stats.name);
        pthread_mutex_unlock(&shared_data.mutex);
        
        // Simulate some processing time
        usleep(10000); // 10ms
    }
    
    printf("\n[THREAD TERMINATING] %s completed\n", odd_stats.name);
    printf("    Numbers printed: %d\n", odd_stats.numbers_printed);
    printf("    Context switches: %d\n", odd_stats.context_switches);
    printf("    Wait count: %d\n\n", odd_stats.wait_count);
    
    return NULL;
}

// Thread function to print even numbers
void* print_even(void* arg) {
    printf("\n[THREAD CREATED] %s (TID: %ld) - State: READY\n", 
           even_stats.name, pthread_self());
    
    while (1) {
        // Acquire mutex lock
        pthread_mutex_lock(&shared_data.mutex);
        printf("    [LOCK ACQUIRED] %s acquired mutex\n", even_stats.name);
        
        // Wait until it's even's turn
        while (shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            printf("    [WAITING] %s waiting for condition (counter=%d)\n", 
                   even_stats.name, shared_data.counter);
            even_stats.wait_count++;
            
            // Thread goes to WAITING state
            printf("    [STATE CHANGE] %s: RUNNING -> WAITING\n", even_stats.name);
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
            printf("    [STATE CHANGE] %s: WAITING -> READY -> RUNNING\n", even_stats.name);
            
            simulate_context_switch(&even_stats);
        }
        
        // Check if we've reached the limit
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Critical section: Print even number
        printf("\n>>> [%s] Printing: %d\n", even_stats.name, shared_data.counter);
        even_stats.numbers_printed++;
        
        // Update shared state
        shared_data.counter++;
        shared_data.is_odd_turn = true;
        
        // Signal odd thread
        printf("    [SIGNAL] %s signaling ODD_THREAD\n", even_stats.name);
        pthread_cond_signal(&shared_data.cond);
        
        // Release mutex
        printf("    [LOCK RELEASED] %s released mutex\n\n", even_stats.name);
        pthread_mutex_unlock(&shared_data.mutex);
        
        // Simulate some processing time
        usleep(10000); // 10ms
    }
    
    printf("\n[THREAD TERMINATING] %s completed\n", even_stats.name);
    printf("    Numbers printed: %d\n", even_stats.numbers_printed);
    printf("    Context switches: %d\n", even_stats.context_switches);
    printf("    Wait count: %d\n\n", even_stats.wait_count);
    
    return NULL;
}

int main() {
    pthread_t odd_thread, even_thread;
    
    printf("\n========================================\n");
    printf("  Linux Thread Scheduler Simulation\n");
    printf("========================================\n");
    printf("Demonstrating:\n");
    printf("  - Thread creation and management\n");
    printf("  - Mutex locks for synchronization\n");
    printf("  - Condition variables for coordination\n");
    printf("  - Thread state transitions\n");
    printf("  - Context switching\n");
    printf("========================================\n");
    
    // Initialize shared data
    shared_data.counter = 1;
    shared_data.is_odd_turn = true;
    pthread_mutex_init(&shared_data.mutex, NULL);
    pthread_cond_init(&shared_data.cond, NULL);
    
    printf("\n[MAIN THREAD] Initializing synchronization primitives\n");
    printf("    Mutex initialized\n");
    printf("    Condition variable initialized\n");
    printf("    Starting counter: %d\n", shared_data.counter);
    
    // Create threads
    printf("\n[MAIN THREAD] Creating worker threads...\n");
    
    if (pthread_create(&odd_thread, NULL, print_odd, NULL) != 0) {
        perror("Failed to create odd thread");
        return 1;
    }
    
    if (pthread_create(&even_thread, NULL, print_even, NULL) != 0) {
        perror("Failed to create even thread");
        return 1;
    }
    
    printf("\n[MAIN THREAD] Both threads created successfully\n");
    printf("[MAIN THREAD] State: RUNNING -> WAITING (waiting for threads to join)\n");
    
    // Wait for threads to complete (join)
    printf("\n[MAIN THREAD] Waiting for ODD_THREAD to join...\n");
    pthread_join(odd_thread, NULL);
    printf("[MAIN THREAD] ODD_THREAD joined successfully\n");
    
    printf("\n[MAIN THREAD] Waiting for EVEN_THREAD to join...\n");
    pthread_join(even_thread, NULL);
    printf("[MAIN THREAD] EVEN_THREAD joined successfully\n");
    
    // Cleanup
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond);
    
    // Print final statistics
    printf("\n========================================\n");
    printf("  Execution Complete - Final Statistics\n");
    printf("========================================\n");
    printf("Final counter value: %d\n\n", shared_data.counter);
    
    printf("ODD_THREAD Statistics:\n");
    printf("  Numbers printed: %d\n", odd_stats.numbers_printed);
    printf("  Context switches: %d\n", odd_stats.context_switches);
    printf("  Wait operations: %d\n\n", odd_stats.wait_count);
    
    printf("EVEN_THREAD Statistics:\n");
    printf("  Numbers printed: %d\n", even_stats.numbers_printed);
    printf("  Context switches: %d\n", even_stats.context_switches);
    printf("  Wait operations: %d\n\n", even_stats.wait_count);
    
    printf("Total context switches: %d\n", 
           odd_stats.context_switches + even_stats.context_switches);
    printf("========================================\n");
    
    printf("\n[MAIN THREAD] Exiting...\n\n");
    
    return 0;
}
