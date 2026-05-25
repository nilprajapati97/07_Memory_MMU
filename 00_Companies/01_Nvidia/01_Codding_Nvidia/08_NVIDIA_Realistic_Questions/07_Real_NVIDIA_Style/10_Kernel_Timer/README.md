# Kernel Timers in the Linux Kernel

## Problem
How do you implement a timer in the Linux kernel? What are the key APIs and considerations?

## Solution Overview
- Use `struct timer_list` and `timer_setup()` to initialize a timer.
- Use `mod_timer()` to schedule the timer.
- Provide a callback function for timer expiry.

## Key Considerations
- Timers run in softirq context; avoid blocking operations.
- Always delete timers with `del_timer_sync()` on module exit.

## Interview Notes
- Discuss timer accuracy, jiffies, and timer wheel.
- Be ready to explain timer context and race conditions.
