# Q30: Explain poll/select/epoll support in driver

## In-depth Explanation (Nvidia Interview Style)

- `poll`, `select`, and `epoll` allow user-space to wait for events on file descriptors.
- Drivers implement the `poll` file operation to support these mechanisms.
- Used for efficient I/O multiplexing.

### Interview Tips
- Be ready to discuss how to implement the `poll` method in a driver.
- Know about event notification and readiness masks.
