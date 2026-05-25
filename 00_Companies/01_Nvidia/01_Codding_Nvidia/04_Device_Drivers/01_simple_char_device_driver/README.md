# Q26: Write a simple character device driver

## Code
See solution.c for a minimal Linux char device driver skeleton.

## In-depth Explanation (Nvidia Interview Style)

A character device driver allows user-space applications to interact with hardware or kernel data as a stream of bytes. The Linux kernel provides a flexible framework for implementing char drivers using file operations.

### Key Steps
- Register a character device with `register_chrdev`.
- Implement file operations: `open`, `read`, `write`, `release`.
- Unregister the device on module exit.

### Why is this important for Nvidia/Linux Kernel?
- Device drivers are essential for hardware interaction.
- Understanding char drivers is foundational for more complex drivers (block, network, GPU, etc).

### Interview Tips
- Be ready to discuss major/minor numbers, file operations, and user-kernel data transfer.
