# Simple NUMA Demo: How to Run

This directory contains a simple C program that demonstrates NUMA-aware memory allocation on Linux using `libnuma`.

## Requirements
- Linux system with NUMA support (multiple nodes)
- `libnuma-dev` package (install with `sudo apt-get install libnuma-dev`)

## Build
```sh
gcc -o simple_numa_demo simple_numa_demo.c -lnuma
```

## Run
```sh
numactl --cpunodebind=0 --membind=0 ./simple_numa_demo
```

## What It Does
- Allocates 1MB of memory on NUMA node 0
- Touches the memory to ensure allocation
- Prints the NUMA node where the memory physically resides
- Prints the CPU and NUMA node the process is running on

## Output Example
```
Allocated 1MB on NUMA node 0
Page is physically on node: 0
Process running on CPU: 0
Process NUMA node: 0
```

Try changing the node in `numactl` to see the effect!
