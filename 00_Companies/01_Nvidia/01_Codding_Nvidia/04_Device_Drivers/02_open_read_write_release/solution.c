// Example open, read, write, release for char device
tatic int dev_open(struct inode *inode, struct file *file) { return 0; }
static int dev_release(struct inode *inode, struct file *file) { return 0; }
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    // Example: return 0 bytes
    return 0;
}
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    // Example: accept all bytes
    return len;
}
