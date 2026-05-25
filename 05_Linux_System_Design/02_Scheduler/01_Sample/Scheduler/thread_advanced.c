#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define MAX_NUMBER 100
#define TIME_QUANTUM_MS 10

// Thread states (Linux kernel thread states)
typedef enum {
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} ThreadState;

// Thread Control Block (TCB) - Similar to task_struct in Linux
typedef struct {
    pthread_t tid;
    int thread_id;
    char name[30];
    ThreadState state;
    int priority;
    long long vruntime;
    int cpu_time;
    int wait_time;
    int numbers_printed;
    int mutex_acquisitions;
    int condition_waits;
    struct timeval start_time;
    struct timeval end_time;
} ThreadControlBlock;

// Shared data structure
typedef struct {
    int counter;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_odd_turn;
    int total_context_switches;
} SharedData;

// Global variables
SharedData shared_data;
ThreadControlBlock odd_tcb;
ThreadControlBlock even_tcb;
pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;

// Get current time in milliseconds
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Initialize Thread Control Block
void init_tcb(ThreadControlBlock* tcb, int id, const char* name, int priority) {
    tcb->thread_id = id;
    snprintf(tcb->name, sizeof(tcb->name), "%s", name);
    tcb->state = THREAD_NEW;
    tcb->priority = priority;
    tcb->vruntime = 0;
    tcb->cpu_time = 0;
    tcb->wait_time = 0;
    tcb->numbers_printed = 0;
    tcb->mutex_acquisitions = 0;
    tcb->condition_waits = 0;
    gettimeofday(&tcb->start_time, NULL);
}

// Change thread state with logging
void change_state(ThreadControlBlock* tcb, ThreadState new_state) {
    const char* state_names[] = {"NEW", "READY", "RUNNING", "WAITING", "TERMINATED"};
    
    pthread_mutex_lock(&scheduler_mutex);
    printf("    [SCHEDULER] %s: %s -> %s\n", 
           tcb->name, state_names[tcb->state], state_names[new_state]);
    tcb->state = new_state;
    pthread_mutex_unlock(&scheduler_mutex);
}

// Simulate context switch
void context_switch(ThreadControlBlock* tcb) {
    pthread_mutex_lock(&scheduler_mutex);
    shared_data.total_context_switches++;
    printf("    [CONTEXT SWITCH #%d] Switching from %s (vruntime=%lld)\n",
           shared_data.total_context_switches, tcb->name, tcb->vruntime);
    pthread_mutex_unlock(&scheduler_mutex);
}

// Display thread statistics
void display_tcb(ThreadControlBlock* tcb) {
    printf("\n  Thread: %s (TID: %lu)\n", tcb->name, (unsigned long)tcb->tid);
    printf("    Priority: %d\n", tcb->priority);
    printf("    Virtual Runtime: %lld\n", tcb->vruntime);
    printf("    CPU Time: %d ms\n", tcb->cpu_time);
    printf("    Wait Time: %d ms\n", tcb->wait_time);
    printf("    Numbers Printed: %d\n", tcb->numbers_printed);
    printf("    Mutex Acquisitions: %d\n", tcb->mutex_acquisitions);
    printf("    Condition Waits: %d\n", tcb->condition_waits);
}

// Odd thread function
void* print_odd(void* arg) {
    ThreadControlBlock* tcb = (ThreadControlBlock*)arg;
    tcb->tid = pthread_self();
    
    change_state(tcb, THREAD_READY);
    printf("\n[THREAD START] %s created (TID: %lu)\n", tcb->name, (unsigned long)tcb->tid);
    
    while (1) {
        long long start = get_time_ms();
        
        // Acquire mutex (enter critical section)
        change_state(tcb, THREAD_RUNNING);
        pthread_mutex_lock(&shared_data.mutex);
        tcb->mutex_acquisitions++;
        
        printf("\n[MUTEX] %s acquired lock\n", tcb->name);
        
        // Wait for turn
        while (!shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            change_state(tcb, THREAD_WAITING);
            printf("[WAIT] %s waiting on condition variable (counter=%d)\n", 
                   tcb->name, shared_data.counter);
            
            tcb->condition_waits++;
            long long wait_start = get_time_ms();
            
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
            
            tcb->wait_time += (get_time_ms() - wait_start);
            change_state(tcb, THREAD_READY);
            context_switch(tcb);
            change_state(tcb, THREAD_RUNNING);
        }
        
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Critical section: print number
        printf("[EXECUTE] %s printing: %d\n", tcb->name, shared_data.counter);
        tcb->numbers_printed++;
        
        // Update shared state
        shared_data.counter++;
        shared_data.is_odd_turn = false;
        
        // Update vruntime (CFS scheduling)
        int exec_time = TIME_QUANTUM_MS;
        tcb->vruntime += exec_time * (20 - tcb->priority);
        tcb->cpu_time += exec_time;
        
        // Signal other thread
        printf("[SIGNAL] %s signaling EVEN thread\n", tcb->name);
        pthread_cond_signal(&shared_data.cond);
        
        pthread_mutex_unlock(&shared_data.mutex);
        printf("[MUTEX] %s released lock\n", tcb->name);
        
        change_state(tcb, THREAD_READY);
        usleep(TIME_QUANTUM_MS * 1000);
    }
    
    change_state(tcb, THREAD_TERMINATED);
    gettimeofday(&tcb->end_time, NULL);
    printf("\n[THREAD END] %s terminated\n", tcb->name);
    
    return NULL;
}

// Even thread function
void* print_even(void* arg) {
    ThreadControlBlock* tcb = (ThreadControlBlock*)arg;
    tcb->tid = pthread_self();
    
    change_state(tcb, THREAD_READY);
    printf("\n[THREAD START] %s created (TID: %lu)\n", tcb->name, (unsigned long)tcb->tid);
    
    while (1) {
        long long start = get_time_ms();
        
        // Acquire mutex (enter critical section)
        change_state(tcb, THREAD_RUNNING);
        pthread_mutex_lock(&shared_data.mutex);
        tcb->mutex_acquisitions++;
        
        printf("\n[MUTEX] %s acquired lock\n", tcb->name);
        
        // Wait for turn
        while (shared_data.is_odd_turn && shared_data.counter <= MAX_NUMBER) {
            change_state(tcb, THREAD_WAITING);
            printf("[WAIT] %s waiting on condition variable (counter=%d)\n", 
                   tcb->name, shared_data.counter);
            
            tcb->condition_waits++;
            long long wait_start = get_time_ms();
            
            pthread_cond_wait(&shared_data.cond, &shared_data.mutex);
            
            tcb->wait_time += (get_time_ms() - wait_start);
            change_state(tcb, THREAD_READY);
            context_switch(tcb);
            change_state(tcb, THREAD_RUNNING);
        }
        
        if (shared_data.counter > MAX_NUMBER) {
            pthread_mutex_unlock(&shared_data.mutex);
            break;
        }
        
        // Critical section: print number
        printf("[EXECUTE] %s printing: %d\n", tcb->name, shared_data.counter);
        tcb->numbers_printed++;
        
        // Update shared state
        shared_data.counter++;
        shared_data.is_odd_turn = true;
        
        // Update vruntime (CFS scheduling)
        int exec_time = TIME_QUANTUM_MS;
        tcb->vruntime += exec_time * (20 - tcb->priority);
        tcb->cpu_time += exec_time;
        
        // Signal other thread
        printf("[SIGNAL] %s signaling ODD thread\n", tcb->name);
        pthread_cond_signal(&shared_data.cond);
        
        pthread_mutex_unlock(&shared_data.mutex);
        printf("[MUTEX] %s released lock\n", tcb->name);
        
        change_state(tcb, THREAD_READY);
        usleep(TIME_QUANTUM_MS * 1000);
    }
    
    change_state(tcb, THREAD_TERMINATED);
    gettimeofday(&tcb->end_time, NULL);
    printf("\n[THREAD END] %s terminated\n", tcb->name);
    
    return NULL;
}

int main() {
    printf("\n");
    printf("================================================================\n");
    printf("     Linux Thread Scheduler - Complete Implementation\n");
    printf("================================================================\n");
    printf("Demonstrating:\n");
    printf("  • Thread Control Blocks (TCB)\n");
    printf("  • Thread State Transitions (NEW->READY->RUNNING->WAITING)\n");
    printf("  • Mutex Locks (Critical Section Protection)\n");
    printf("  • Condition Variables (Thread Synchronization)\n");
    printf("  • Context Switching\n");
    printf("  • Virtual Runtime (CFS Scheduling)\n");
    printf("  • Priority-based Scheduling\n");
    printf("================================================================\n\n");
    
    // Initialize shared data
    shared_data.counter = 1;
    shared_data.is_odd_turn = true;
    shared_data.total_context_switches = 0;
    pthread_mutex_init(&shared_data.mutex, NULL);
    pthread_cond_init(&shared_data.cond, NULL);
    
    // Initialize Thread Control Blocks
    init_tcb(&odd_tcb, 1, "ODD_THREAD", 15);
    init_tcb(&even_tcb, 2, "EVEN_THREAD", 15);
    
    printf("[MAIN] Initializing synchronization primitives...\n");
    printf("  • Mutex initialized\n");
    printf("  • Condition variable initialized\n");
    printf("  • Starting counter: %d\n", shared_data.counter);
    printf("  • Max number: %d\n\n", MAX_NUMBER);
    
    // Create threads
    printf("[MAIN] Creating threads...\n");
    pthread_create(&odd_tcb.tid, NULL, print_odd, &odd_tcb);
    pthread_create(&even_tcb.tid, NULL, print_even, &even_tcb);
    
    printf("\n[MAIN] Main thread entering WAITING state (pthread_join)...\n");
    
    // Join threads
    pthread_join(odd_tcb.tid, NULL);
    printf("\n[MAIN] ODD_THREAD joined to main thread\n");
    
    pthread_join(even_tcb.tid, NULL);
    printf("[MAIN] EVEN_THREAD joined to main thread\n");
    
    // Cleanup
    pthread_mutex_destroy(&shared_data.mutex);
    pthread_cond_destroy(&shared_data.cond);
    
    // Display final statistics
    printf("\n");
    printf("================================================================\n");
    printf("                    Final Statistics\n");
    printf("================================================================\n");
    printf("Final Counter: %d\n", shared_data.counter);
    printf("Total Context Switches: %d\n", shared_data.total_context_switches);
    
    display_tcb(&odd_tcb);
    display_tcb(&even_tcb);
    
    printf("\n================================================================\n");
    printf("Key Observations:\n");
    printf("================================================================\n");
    printf("1. Threads alternate execution using condition variables\n");
    printf("2. Mutex ensures only one thread in critical section\n");
    printf("3. Context switches occur when threads wait/signal\n");
    printf("4. Virtual runtime tracks CPU time for fair scheduling\n");
    printf("5. Threads join back to main before program exits\n");
    printf("================================================================\n\n");
    
    printf("[MAIN] All threads completed. Main thread exiting...\n\n");
    
    return 0;
}
