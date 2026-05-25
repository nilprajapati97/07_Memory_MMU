#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PROCESSES 10
#define TIME_QUANTUM 4
#define NICE_0_WEIGHT 1024

typedef enum {
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

typedef enum {
    SCHED_NORMAL,
    SCHED_FIFO,
    SCHED_RR
} SchedPolicy;

typedef struct {
    int pid;
    char name[20];
    int priority;           // 0-19 (higher = more priority)
    int nice;               // -20 to +19 (lower = higher priority)
    int burst_time;
    int remaining_time;
    long long vruntime;     // Virtual runtime for CFS
    int time_slice;         // Current time slice
    ProcessState state;
    SchedPolicy policy;
    int cpu_affinity;       // Which CPU core
    int wait_time;          // Time spent waiting
    int turnaround_time;    // Total time in system
} Process;

typedef struct {
    Process* processes[MAX_PROCESSES];
    int count;
    long long min_vruntime; // Track minimum vruntime
} RunQueue;

typedef struct {
    int total_context_switches;
    int total_time;
    float avg_wait_time;
    float avg_turnaround_time;
} SchedulerStats;

// Initialize process with all parameters
void init_process(Process* p, int pid, const char* name, int priority, int burst_time, SchedPolicy policy) {
    p->pid = pid;
    strncpy(p->name, name, 19);
    p->name[19] = '\0';
    p->priority = priority;
    p->nice = 20 - priority;  // Convert priority to nice value
    p->burst_time = burst_time;
    p->remaining_time = burst_time;
    p->vruntime = 0;
    p->time_slice = TIME_QUANTUM;
    p->state = READY;
    p->policy = policy;
    p->cpu_affinity = 0;
    p->wait_time = 0;
    p->turnaround_time = 0;
}

// Add process to run queue
void add_to_queue(RunQueue* queue, Process* p) {
    if (queue->count < MAX_PROCESSES) {
        queue->processes[queue->count++] = p;
        printf("[SCHEDULER] Process %s (PID:%d) added to run queue\n", p->name, p->pid);
    }
}

// CFS: Pick process with minimum vruntime
Process* pick_next_task(RunQueue* queue) {
    if (queue->count == 0) return NULL;
    
    int min_idx = 0;
    for (int i = 1; i < queue->count; i++) {
        if (queue->processes[i]->vruntime < queue->processes[min_idx]->vruntime) {
            min_idx = i;
        }
    }
    return queue->processes[min_idx];
}

// Remove terminated process from queue
void remove_from_queue(RunQueue* queue, Process* p) {
    for (int i = 0; i < queue->count; i++) {
        if (queue->processes[i]->pid == p->pid) {
            for (int j = i; j < queue->count - 1; j++) {
                queue->processes[j] = queue->processes[j + 1];
            }
            queue->count--;
            break;
        }
    }
}

// Simulate context switch
void context_switch(Process* prev, Process* next, SchedulerStats* stats) {
    if (prev) {
        printf("             [CONTEXT SWITCH] Saving state of %s\n", prev->name);
        prev->state = READY;
    }
    if (next) {
        printf("             [CONTEXT SWITCH] Loading state of %s\n", next->name);
        next->state = RUNNING;
    }
    stats->total_context_switches++;
}

// Update wait times for all processes in queue
void update_wait_times(RunQueue* queue, Process* current, int exec_time) {
    for (int i = 0; i < queue->count; i++) {
        if (queue->processes[i]->pid != current->pid && 
            queue->processes[i]->state == READY) {
            queue->processes[i]->wait_time += exec_time;
        }
    }
}

// Display current run queue state
void display_queue_state(RunQueue* queue) {
    printf("\n[RUN QUEUE STATE] Processes: %d\n", queue->count);
    for (int i = 0; i < queue->count; i++) {
        Process* p = queue->processes[i];
        printf("  PID:%d %-12s | VRuntime:%-4lld | Remaining:%-2d | Priority:%-2d\n",
               p->pid, p->name, p->vruntime, p->remaining_time, p->priority);
    }
    printf("\n");
}

// Main scheduler function
void schedule(RunQueue* queue, SchedulerStats* stats) {
    int time = 0;
    int process_count = queue->count;
    
    printf("\n========================================\n");
    printf("   Linux CFS Scheduler Simulation\n");
    printf("========================================\n");
    printf("Time Quantum: %d units\n", TIME_QUANTUM);
    printf("Total Processes: %d\n", process_count);
    printf("========================================\n\n");
    
    display_queue_state(queue);
    
    while (queue->count > 0) {
        Process* current = pick_next_task(queue);
        if (!current) break;
        
        printf("\n[TIME %3d] Running Process: %s (PID:%d)\n", time, current->name, current->pid);
        printf("           Priority: %d | Nice: %d | VRuntime: %lld\n",
               current->priority, current->nice, current->vruntime);
        printf("           Remaining Time: %d | Wait Time: %d\n",
               current->remaining_time, current->wait_time);
        
        current->state = RUNNING;
        
        // Calculate execution time
        int exec_time = (current->remaining_time < TIME_QUANTUM) ? 
                        current->remaining_time : TIME_QUANTUM;
        
        // Execute process
        current->remaining_time -= exec_time;
        
        // Update vruntime based on priority (CFS algorithm)
        // Higher priority = slower vruntime growth = more CPU time
        int weight = 20 - current->priority;
        current->vruntime += exec_time * weight;
        
        // Update wait times for other processes
        update_wait_times(queue, current, exec_time);
        
        time += exec_time;
        
        // Check if process completed
        if (current->remaining_time == 0) {
            current->state = TERMINATED;
            current->turnaround_time = time;
            
            printf("\n           >>> Process %s COMPLETED <<<\n", current->name);
            printf("           Total Wait Time: %d\n", current->wait_time);
            printf("           Turnaround Time: %d\n", current->turnaround_time);
            
            stats->avg_wait_time += current->wait_time;
            stats->avg_turnaround_time += current->turnaround_time;
            
            remove_from_queue(queue, current);
        } else {
            printf("\n           [Time Quantum Expired]\n");
            context_switch(current, NULL, stats);
        }
        
        if (queue->count > 0) {
            display_queue_state(queue);
        }
    }
    
    stats->total_time = time;
    stats->avg_wait_time /= process_count;
    stats->avg_turnaround_time /= process_count;
    
    printf("\n========================================\n");
    printf("   Scheduling Complete\n");
    printf("========================================\n");
    printf("Total Time: %d units\n", stats->total_time);
    printf("Context Switches: %d\n", stats->total_context_switches);
    printf("Average Wait Time: %.2f units\n", stats->avg_wait_time);
    printf("Average Turnaround Time: %.2f units\n", stats->avg_turnaround_time);
    printf("========================================\n\n");
}

int main() {
    Process p1, p2, p3, p4;
    RunQueue queue = {.count = 0, .min_vruntime = 0};
    SchedulerStats stats = {0, 0, 0.0, 0.0};
    
    printf("\n[SYSTEM] Initializing processes...\n\n");
    
    // Create processes with different priorities
    init_process(&p1, 1, "WebBrowser", 15, 12, SCHED_NORMAL);
    init_process(&p2, 2, "VideoPlayer", 10, 8, SCHED_NORMAL);
    init_process(&p3, 3, "TextEditor", 18, 6, SCHED_NORMAL);
    init_process(&p4, 4, "Compiler", 12, 10, SCHED_NORMAL);
    
    // Add to run queue
    add_to_queue(&queue, &p1);
    add_to_queue(&queue, &p2);
    add_to_queue(&queue, &p3);
    add_to_queue(&queue, &p4);
    
    // Run scheduler
    schedule(&queue, &stats);
    
    return 0;
}
