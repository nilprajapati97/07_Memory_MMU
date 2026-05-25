# Deferred Work Using Workqueues in the Linux Kernel

## Problem
How do you defer work from interrupt context to process context in the Linux kernel? What are the APIs and considerations?

## Solution Overview
- Use `work_struct` and `INIT_WORK()` to define deferred work.
- Use `schedule_work()` to queue work for execution in process context.
- Use `flush_scheduled_work()` to wait for completion.

## Key Considerations
- Workqueues run in process context, allowing sleeping/blocking operations.
- Always flush or cancel work on module exit.

## Interview Notes
- Discuss differences between tasklets, workqueues, and timers.
- Be ready to explain context transitions and concurrency.
