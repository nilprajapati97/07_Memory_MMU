# Stack Overflow Detection in the Linux Kernel

## Problem
How do you detect stack overflow risk in the Linux kernel? What are the APIs and best practices?

## Solution Overview
- Compare the current stack pointer (`sp`) to the stack boundaries.
- Warn if `sp` is near the start or end of the stack.

## Best Practices
- Use margin checks to detect overflow risk early.
- Avoid deep recursion and large stack allocations in kernel code.

## Interview Notes
- Discuss kernel stack size, guard pages, and stacktrace APIs.
- Be ready to explain stack usage analysis and debugging.
