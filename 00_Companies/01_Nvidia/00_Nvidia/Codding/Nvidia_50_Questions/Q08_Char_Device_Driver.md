# Q08: Character Device Driver with read / write / ioctl

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** `cdev`, `file_operations`, `copy_to_user`, `copy_from_user`, ioctl, device registration

---

## Question

Implement a character device driver with `read`, `write`, and `ioctl` support.

---

## Answer

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME  "nvidia_sample"
#define BUF_SIZE     4096

/* ioctl command definitions */
#define IOC_MAGIC   'N'
#define IOC_RESET   _IO(IOC_MAGIC,  1)          /* no arg */
#define IOC_GETSTAT _IOR(IOC_MAGIC, 2, u64)     /* read u64 from driver */
#define IOC_SETCONF _IOW(IOC_MAGIC, 3, u32)     /* write u32 to driver */

struct sample_dev {
    char         buf[BUF_SIZE];
    size_t       buf_len;
    struct cdev  cdev;
    struct mutex lock;
};

static struct sample_dev *sample;
static dev_t              dev_num;   /* major:minor */
static struct class      *dev_class;

/* ─── open ────────────────────────────────────────────────────────────────*/
static int sample_open(struct inode *inode, struct file *filp)
{
    struct sample_dev *dev = container_of(inode->i_cdev,
                                           struct sample_dev, cdev);
    filp->private_data = dev;
    return 0;
}

/* ─── read ────────────────────────────────────────────────────────────────*/
static ssize_t sample_read(struct file *filp, char __user *ubuf,
                             size_t len, loff_t *off)
{
    struct sample_dev *dev = filp->private_data;
    ssize_t ret;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*off >= dev->buf_len) {
        ret = 0; /* EOF */
        goto out;
    }

    len = min(len, dev->buf_len - (size_t)*off);

    /* copy_to_user: safe transfer from kernel to userspace */
    if (copy_to_user(ubuf, dev->buf + *off, len)) {
        ret = -EFAULT;
        goto out;
    }

    *off += len;
    ret = len;
out:
    mutex_unlock(&dev->lock);
    return ret;
}

/* ─── write ───────────────────────────────────────────────────────────────*/
static ssize_t sample_write(struct file *filp, const char __user *ubuf,
                              size_t len, loff_t *off)
{
    struct sample_dev *dev = filp->private_data;
    ssize_t ret;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (*off >= BUF_SIZE) {
        ret = -ENOSPC;
        goto out;
    }

    len = min(len, (size_t)(BUF_SIZE - *off));

    /* copy_from_user: safe transfer from userspace to kernel */
    if (copy_from_user(dev->buf + *off, ubuf, len)) {
        ret = -EFAULT;
        goto out;
    }

    *off       += len;
    dev->buf_len = max(dev->buf_len, (size_t)*off);
    ret          = len;
out:
    mutex_unlock(&dev->lock);
    return ret;
}

/* ─── ioctl ───────────────────────────────────────────────────────────────*/
static long sample_ioctl(struct file *filp, unsigned int cmd,
                          unsigned long arg)
{
    struct sample_dev *dev = filp->private_data;
    u64 stat;
    u32 conf;

    /* Validate magic number and command number */
    if (_IOC_TYPE(cmd) != IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > 3)
        return -ENOTTY;

    switch (cmd) {
    case IOC_RESET:
        mutex_lock(&dev->lock);
        memset(dev->buf, 0, BUF_SIZE);
        dev->buf_len = 0;
        mutex_unlock(&dev->lock);
        return 0;

    case IOC_GETSTAT:
        stat = dev->buf_len;
        /* copy_to_user for ioctl output */
        if (copy_to_user((void __user *)arg, &stat, sizeof(stat)))
            return -EFAULT;
        return 0;

    case IOC_SETCONF:
        /* copy_from_user for ioctl input */
        if (copy_from_user(&conf, (void __user *)arg, sizeof(conf)))
            return -EFAULT;
        /* validate conf before use */
        if (conf > 100)
            return -EINVAL;
        pr_info("Config set: %u\n", conf);
        return 0;

    default:
        return -ENOTTY;
    }
}

/* ─── file_operations table ───────────────────────────────────────────────*/
static const struct file_operations sample_fops = {
    .owner          = THIS_MODULE,
    .open           = sample_open,
    .read           = sample_read,
    .write          = sample_write,
    .unlocked_ioctl = sample_ioctl,
    .llseek         = default_llseek,
};

/* ─── module init / exit ──────────────────────────────────────────────────*/
static int __init sample_init(void)
{
    int ret;

    sample = kzalloc(sizeof(*sample), GFP_KERNEL);
    if (!sample)
        return -ENOMEM;
    mutex_init(&sample->lock);

    /* Allocate dynamic major:minor */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret)
        goto err_alloc;

    cdev_init(&sample->cdev, &sample_fops);
    sample->cdev.owner = THIS_MODULE;
    ret = cdev_add(&sample->cdev, dev_num, 1);
    if (ret)
        goto err_cdev;

    /* Create /dev/nvidia_sample automatically via udev */
    dev_class = class_create(DEVICE_NAME);
    device_create(dev_class, NULL, dev_num, NULL, DEVICE_NAME);

    pr_info("nvidia_sample: major=%d minor=%d\n",
            MAJOR(dev_num), MINOR(dev_num));
    return 0;

err_cdev:
    unregister_chrdev_region(dev_num, 1);
err_alloc:
    kfree(sample);
    return ret;
}

static void __exit sample_exit(void)
{
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&sample->cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(sample);
}

module_init(sample_init);
module_exit(sample_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NVIDIA");
```

---

## Explanation

### Core Concept

A Linux character device exposes a file interface (`/dev/xxx`) that userspace programs can `open()`, `read()`, `write()`, and `ioctl()`. The driver registers a `file_operations` struct mapping system calls to driver functions.

**Data flow:**
```
Userspace         syscall          Kernel driver
─────────         ───────          ─────────────
read(fd, buf, n) ─────────────► sample_read()
                                  copy_to_user(buf, kbuf, n)
                 ◄───────────────
write(fd, buf,n) ─────────────► sample_write()
                                  copy_from_user(kbuf, buf, n)
ioctl(fd, cmd)   ─────────────► sample_ioctl()
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `alloc_chrdev_region(&dev, 0, 1, name)` | Dynamically allocate major:minor |
| `cdev_init(&cdev, &fops)` | Link cdev to file_operations |
| `cdev_add(&cdev, dev, count)` | Register cdev with the kernel |
| `class_create(name)` | Create device class for udev |
| `device_create(class, ...)` | Create `/dev/` entry automatically |
| `copy_to_user(ubuf, kbuf, n)` | Safe kernel → userspace copy |
| `copy_from_user(kbuf, ubuf, n)` | Safe userspace → kernel copy |
| `_IO(magic, nr)` | ioctl with no argument |
| `_IOR(magic, nr, type)` | ioctl reading `type` from driver |
| `_IOW(magic, nr, type)` | ioctl writing `type` to driver |
| `_IOWR(magic, nr, type)` | ioctl read+write `type` |

### Trade-offs & Pitfalls

- **Never dereference `__user` pointers directly.** Userspace memory can fault, be remapped, or be from a malicious process. Always use `copy_from_user`/`copy_to_user` which validate the address and handle faults safely.
- **Validate ioctl command numbers.** Return `-ENOTTY` for unknown commands. Check `_IOC_TYPE` and `_IOC_NR` bounds. Without validation, an attacker can pass arbitrary commands to the `switch` statement — integer overflow attacks.
- **`unlocked_ioctl` vs `ioctl`:** The old `ioctl` field held the BKL (Big Kernel Lock). Modern drivers use `unlocked_ioctl` (no BKL). For 32-bit userspace on 64-bit kernel, also implement `compat_ioctl`.
- **`filp->private_data`:** Store your per-file driver state here. Retrieved in every `read`/`write`/`ioctl` call.

### NVIDIA / GPU Context

The NVIDIA GPU driver exposes `/dev/nvidia0`, `/dev/nvidiactl`, `/dev/nvidia-uvm` as character devices. Key ioctls include:
- `NV_ESC_ALLOC_OS_EVENT` — allocate GPU event objects
- `NV_ESC_RM_ALLOC` — allocate GPU resources (channels, memory)
- `NV_ESC_RM_CONTROL` — control GPU hardware parameters
All NVIDIA ioctls copy a fixed-size header from userspace first, then dispatch to internal resource manager (RM) functions.

---

## Cross Questions & Answers

**CQ1: What happens if `copy_from_user` returns a non-zero value?**
> `copy_from_user` returns the number of bytes that could NOT be copied (on failure). Zero means success, non-zero means a partial or full failure — typically due to an invalid or unmapped userspace address. The driver must return `-EFAULT` in this case. Never partially process data if the copy failed — the kernel buffer may contain stale or garbage data.

**CQ2: How does `_IO`, `_IOR`, `_IOW` encoding prevent ioctl command collisions between drivers?**
> The `_IO*` macros encode a magic number (unique per driver), the command number, the direction (read/write), and the argument size into a 32-bit integer. If two drivers accidentally use the same command number, they use different magic bytes, so the full encoded value differs. The kernel's `_IOC_TYPE(cmd)` extracts the magic byte for validation. NVIDIA uses magic `'N'` for its ioctl commands.

**CQ3: What is `compat_ioctl` and when does a GPU driver need it?**
> `compat_ioctl` handles ioctls from a 32-bit userspace process running on a 64-bit kernel. The issue: a `struct` containing a pointer is 4 bytes wide in 32-bit userspace but 8 bytes in 64-bit kernel space. `compat_ioctl` receives the raw 32-bit arg and must manually convert 32-bit pointers/sizes to 64-bit. NVIDIA's driver has extensive `compat_ioctl` handling for its control structures that contain physical address fields.

**CQ4: How would you add `poll`/`select` support to this driver so userspace can wait for GPU events?**
> Implement `file_operations.poll`:
> ```c
> static __poll_t sample_poll(struct file *filp, poll_table *wait) {
>     poll_wait(filp, &dev->event_wq, wait);
>     if (atomic_read(&dev->event_count) > 0)
>         return EPOLLIN | EPOLLRDNORM;
>     return 0;
> }
> ```
> `poll_wait` registers the file's wait queue with the `poll_table`. When the driver calls `wake_up(&event_wq)` (e.g., from an IRQ handler), `poll`/`select`/`epoll` in userspace wakes up.

**CQ5: How does the NVIDIA driver handle the case of multiple processes opening `/dev/nvidia0` simultaneously?**
> Each `open()` creates a new `struct file` with its own `private_data`. The driver allocates a per-file GPU context (with its own VA space, channel, and handle table). This is the `gpu_context` — isolated from other processes. Shared GPU resources (e.g., VRAM heap) are protected by per-device mutexes. The driver tracks all open file handles in a per-device list and cleans up on `release()` (called when the last `fd` referencing the file is closed).
