# Character Device with Asynchronous I/O

## Problem
Write a Linux kernel module that registers a character device and supports asynchronous I/O using poll/select.

## Solution Overview
- Registers a character device with the kernel.
- Implements `read`, `write`, and `poll` file operations.
- Uses a wait queue to support asynchronous notification for poll/select.

## Key Points
- `poll`/`select` allow user-space to wait for data availability.
- `wait_event_interruptible` and `wake_up_interruptible` are used for blocking and waking readers.
- The device buffer is fixed size for simplicity.
