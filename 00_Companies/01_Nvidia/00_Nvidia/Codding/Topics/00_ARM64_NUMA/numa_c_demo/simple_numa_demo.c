/*
 * Simple NUMA-aware memory allocation demo (Linux, C)
 *
 * This program allocates memory on a specific NUMA node and prints the node affinity of the process and the memory.
 * Requires: libnuma-dev (install with: sudo apt-get install libnuma-dev)
 * Compile: gcc -o simple_numa_demo simple_numa_demo.c -lnuma
 * Run: numactl --cpunodebind=0 --membind=0 ./simple_numa_demo
 */
#include <stdio.h>
#include <numa.h>
#include <numaif.h>
#include <unistd.h>

int main() {
    if (numa_available() == -1) {
        printf("NUMA is not available on this system.\n");
        return 1;
    }

    int node = 0;
    size_t size = 1024 * 1024; // 1 MB
    void *mem = numa_alloc_onnode(size, node);
    if (!mem) {
        printf("Failed to allocate memory on node %d\n", node);
        return 1;
    }

    printf("Allocated 1MB on NUMA node %d\n", node);

    // Touch the memory to ensure allocation
    for (size_t i = 0; i < size; i += 4096) {
        ((char*)mem)[i] = 42;
    }

    // Query the node of the allocated memory
    int status;
    move_pages(0, 1, &mem, NULL, &status, 0);
    printf("Page is physically on node: %d\n", status);

    // Print process CPU and node affinity
    printf("Process running on CPU: %d\n", sched_getcpu());
    printf("Process NUMA node: %d\n", numa_node_of_cpu(sched_getcpu()));

    numa_free(mem, size);
    return 0;
}
