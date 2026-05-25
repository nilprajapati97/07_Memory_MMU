/*
 * Linux Kernel Module: Character Device with Async I/O (poll/select)
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DEVICE_NAME "asyncchardev"
#define BUF_SIZE 256

static int major;
static struct cdev async_cdev;
static char device_buf[BUF_SIZE];
static int data_avail = 0;
static wait_queue_head_t wq;

static ssize_t async_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    if (!data_avail)
        if (wait_event_interruptible(wq, data_avail))
            return -ERESTARTSYS;
    if (copy_to_user(buf, device_buf, BUF_SIZE))
        return -EFAULT;
    data_avail = 0;
    return BUF_SIZE;
}

static ssize_t async_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    if (copy_from_user(device_buf, buf, BUF_SIZE))
        return -EFAULT;
    data_avail = 1;
    wake_up_interruptible(&wq);
    return BUF_SIZE;
}

static unsigned int async_poll(struct file *file, poll_table *wait) {
    unsigned int mask = 0;
    poll_wait(file, &wq, wait);
    if (data_avail)
        mask |= POLLIN | POLLRDNORM;
    return mask;
}

static struct file_operations async_fops = {
    .owner = THIS_MODULE,
    .read = async_read,
    .write = async_write,
    .poll = async_poll,
};

static int __init async_init(void) {
    dev_t dev;
    alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    major = MAJOR(dev);
    cdev_init(&async_cdev, &async_fops);
    cdev_add(&async_cdev, dev, 1);
    init_waitqueue_head(&wq);
    pr_info("Async char device loaded\n");
    return 0;
}

static void __exit async_exit(void) {
    cdev_del(&async_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    pr_info("Async char device unloaded\n");
}

module_init(async_init);
module_exit(async_exit);
MODULE_LICENSE("GPL");
