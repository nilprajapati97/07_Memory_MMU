# Q28: Explain ioctl

## In-depth Explanation (Nvidia Interview Style)

- `ioctl` (input/output control) is a system call for device-specific operations not covered by standard file operations.
- Used for configuration, control, and custom commands.
- Requires defining command codes and handling them in the driver.

### Interview Tips
- Be ready to discuss how to define and validate ioctl commands.
- Know about user-kernel data transfer and security implications.
