# Interruptible Waitqueues in the Linux Kernel

## Problem
How do you use an interruptible waitqueue in the Linux kernel? What are the APIs and use cases?

## Solution Overview
- Use `DECLARE_WAIT_QUEUE_HEAD()` to declare a waitqueue.
- Use `wait_event_interruptible()` to sleep until a condition is met or interrupted.
- Use `wake_up_interruptible()` to wake up waiting tasks.

## Use Cases
- Blocking I/O, device drivers, and event notification.
- Allows tasks to be interrupted by signals.

## Interview Notes
- Discuss differences between interruptible and uninterruptible waits.
- Be ready to explain race conditions and wakeup ordering.
