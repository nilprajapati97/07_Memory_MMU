# Completion Synchronization in the Linux Kernel

## Problem
How do you use the completion primitive for synchronization in the Linux kernel? What are the APIs and use cases?

## Solution Overview
- Use `DECLARE_COMPLETION()` to declare a completion object.
- Use `wait_for_completion()` to block until completed.
- Use `complete()` to signal completion from another context.

## Use Cases
- Synchronizing between threads, interrupt handlers, or workqueues.
- Waiting for asynchronous events.

## Interview Notes
- Discuss differences between completions, semaphores, and wait queues.
- Be ready to explain race conditions and completion API details.
