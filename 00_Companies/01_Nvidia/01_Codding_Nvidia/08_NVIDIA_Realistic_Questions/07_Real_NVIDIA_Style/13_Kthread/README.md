# Kernel Threads (kthreads) in the Linux Kernel

## Problem
How do you create and manage a kernel thread (kthread) in the Linux kernel? What are the APIs and considerations?

## Solution Overview
- Use `kthread_run()` to create a kernel thread.
- Implement the thread function with a loop and `kthread_should_stop()`.
- Use `kthread_stop()` to terminate the thread.

## Key Considerations
- Kernel threads run in process context and can sleep/block.
- Always stop threads on module exit.

## Interview Notes
- Discuss differences between kthreads and workqueues.
- Be ready to explain thread lifecycle, synchronization, and cleanup.
