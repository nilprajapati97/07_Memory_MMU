// Memory barrier example
#include <stdatomic.h>

void example_barrier() {
    int a = 0, b = 0;
    atomic_thread_fence(memory_order_seq_cst); // Full barrier
    a = 1;
    atomic_thread_fence(memory_order_release); // Store barrier
    b = 2;
    atomic_thread_fence(memory_order_acquire); // Load barrier
}
