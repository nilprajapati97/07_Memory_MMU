# Q39: Why use readl()/writel() instead of raw pointer access?

## In-depth Explanation (Nvidia Interview Style)

- `readl()`/`writel()` provide safe, ordered access to MMIO registers.
- Handle memory barriers and platform-specific requirements.
- Raw pointer access can lead to subtle bugs and undefined behavior.

### Interview Tips
- Be ready to discuss memory ordering, portability, and register access safety.
